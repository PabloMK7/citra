// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <unordered_map>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "common/archives.h"
#include "common/assert.h"
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/romfs.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/http/http_c.h"
#include "core/hw/aes/key.h"

SERIALIZE_EXPORT_IMPL(Service::HTTP::HTTP_C)
SERIALIZE_EXPORT_IMPL(Service::HTTP::SessionData)

namespace Service::HTTP {

namespace ErrCodes {
enum {
    InvalidRequestState = 22,
    TooManyContexts = 26,
    InvalidRequestMethod = 32,
    ContextNotFound = 100,

    /// This error is returned in multiple situations: when trying to initialize an
    /// already-initialized session, or when using the wrong context handle in a context-bound
    /// session
    SessionStateError = 102,
    TooManyClientCerts = 203,
    NotImplemented = 1012,
};
}

const ResultCode ERROR_STATE_ERROR = // 0xD8A0A066
    ResultCode(ErrCodes::SessionStateError, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);
const ResultCode ERROR_NOT_IMPLEMENTED = // 0xD960A3F4
    ResultCode(ErrCodes::NotImplemented, ErrorModule::HTTP, ErrorSummary::Internal,
               ErrorLevel::Permanent);
const ResultCode ERROR_TOO_MANY_CLIENT_CERTS = // 0xD8A0A0CB
    ResultCode(ErrCodes::TooManyClientCerts, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);
const ResultCode ERROR_WRONG_CERT_ID = // 0xD8E0B839
    ResultCode(57, ErrorModule::SSL, ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
const ResultCode ERROR_WRONG_CERT_HANDLE = // 0xD8A0A0C9
    ResultCode(201, ErrorModule::HTTP, ErrorSummary::InvalidState, ErrorLevel::Permanent);
const ResultCode ERROR_CERT_ALREADY_SET = // 0xD8A0A03D
    ResultCode(61, ErrorModule::HTTP, ErrorSummary::InvalidState, ErrorLevel::Permanent);

static std::pair<std::string, std::string> SplitUrl(const std::string& url) {
    const std::string prefix = "://";
    const auto scheme_end = url.find(prefix);
    const auto prefix_end = scheme_end == std::string::npos ? 0 : scheme_end + prefix.length();

    const auto path_index = url.find("/", prefix_end);
    std::string host;
    std::string path;
    if (path_index == std::string::npos) {
        // If no path is specified after the host, set it to "/"
        host = url;
        path = "/";
    } else {
        host = url.substr(0, path_index);
        path = url.substr(path_index);
    }
    return std::make_pair(host, path);
}

void Context::MakeRequest() {
    ASSERT(state == RequestState::NotStarted);

    const auto& [host, path] = SplitUrl(url);
    const auto client = std::make_unique<httplib::Client>(host);
    SSL_CTX* ctx = client->ssl_context();
    if (ctx) {
        if (auto client_cert = ssl_config.client_cert_ctx.lock()) {
            SSL_CTX_use_certificate_ASN1(ctx, static_cast<int>(client_cert->certificate.size()),
                                         client_cert->certificate.data());
            SSL_CTX_use_PrivateKey_ASN1(EVP_PKEY_RSA, ctx, client_cert->private_key.data(),
                                        static_cast<long>(client_cert->private_key.size()));
        }

        // TODO(B3N30): Check for SSLOptions-Bits and set the verify method accordingly
        // https://www.3dbrew.org/wiki/SSL_Services#SSLOpt
        // Hack: Since for now RootCerts are not implemented we set the VerifyMode to None.
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    }

    state = RequestState::InProgress;

    static const std::unordered_map<RequestMethod, std::string> request_method_strings{
        {RequestMethod::Get, "GET"},       {RequestMethod::Post, "POST"},
        {RequestMethod::Head, "HEAD"},     {RequestMethod::Put, "PUT"},
        {RequestMethod::Delete, "DELETE"}, {RequestMethod::PostEmpty, "POST"},
        {RequestMethod::PutEmpty, "PUT"},
    };

    httplib::Request request;
    httplib::Error error;
    request.method = request_method_strings.at(method);
    request.path = path;
    // TODO(B3N30): Add post data body
    request.progress = [this](u64 current, u64 total) -> bool {
        // TODO(B3N30): Is there a state that shows response header are available
        current_download_size_bytes = current;
        total_download_size_bytes = total;
        return true;
    };

    for (const auto& header : headers) {
        request.headers.emplace(header.name, header.value);
    }

    if (!client->send(request, response, error)) {
        LOG_ERROR(Service_HTTP, "Request failed: {}: {}", error, httplib::to_string(error));
        state = RequestState::TimedOut;
    } else {
        LOG_DEBUG(Service_HTTP, "Request successful");
        // TODO(B3N30): Verify this state on HW
        state = RequestState::ReadyToDownloadContent;
    }
}

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 shmem_size = rp.Pop<u32>();
    u32 pid = rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory) {
        shared_memory->SetName("HTTP_C:shared_memory");
    }

    LOG_WARNING(Service_HTTP, "(STUBBED) called, shared memory size: {} pid: {}", shmem_size, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // This returns 0xd8a0a046 if no network connection is available.
    // Just assume we are always connected.
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::InitializeConnectionSession(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    u32 pid = rp.PopPID();

    LOG_DEBUG(Service_HTTP, "called, context_id={} pid={}", context_handle, pid);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to initialize an already initialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
        return;
    }

    // TODO(Subv): Check that the input PID matches the PID that created the context.
    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::ContextNotFound, ErrorModule::HTTP, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        return;
    }

    session_data->initialized = true;
    session_data->session_id = ++session_counter;
    // Bind the context to the current session.
    session_data->current_http_context = context_handle;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::BeginRequest(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    itr->second.request_future =
        std::async(std::launch::async, &Context::MakeRequest, std::ref(itr->second));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::BeginRequestAsync(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_id={}", context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    // On a 3DS BeginRequest and BeginRequestAsync will push the Request to a worker queue.
    // You can only enqueue 8 requests at the same time.
    // trying to enqueue any more will either fail (BeginRequestAsync), or block (BeginRequest)
    // Note that you only can have 8 Contexts at a time. So this difference shouldn't matter
    // Then there are 3? worker threads that pop the requests from the queue and send them
    // For now make every request async in it's own thread.

    itr->second.request_future =
        std::async(std::launch::async, &Context::MakeRequest, std::ref(itr->second));

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::ReceiveData(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, false);
}

void HTTP_C::ReceiveDataTimeout(Kernel::HLERequestContext& ctx) {
    ReceiveDataImpl(ctx, true);
}

void HTTP_C::ReceiveDataImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 buffer_size = rp.Pop<u32>();
    u64 timeout_nanos = 0;
    if (timeout) {
        timeout_nanos = rp.Pop<u64>();
        LOG_WARNING(Service_HTTP, "(STUBBED) called, timeout={}", timeout_nanos);
    } else {
        LOG_WARNING(Service_HTTP, "(STUBBED) called");
    }
    [[maybe_unused]] Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    if (timeout) {
        itr->second.request_future.wait_for(std::chrono::nanoseconds(timeout_nanos));
        // TODO (flTobi): Return error on timeout
    } else {
        itr->second.request_future.wait();
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 url_size = rp.Pop<u32>();
    RequestMethod method = rp.PopEnum<RequestMethod>();
    Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

    // Copy the buffer into a string without the \0 at the end of the buffer
    std::string url(url_size, '\0');
    buffer.Read(&url[0], 0, url_size - 1);

    LOG_DEBUG(Service_HTTP, "called, url_size={}, url={}, method={}", url_size, url, method);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    // This command can only be called without a bound session.
    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrorDescription::NotImplemented, ErrorModule::HTTP,
                           ErrorSummary::Internal, ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }

    static constexpr std::size_t MaxConcurrentHTTPContexts = 8;
    if (session_data->num_http_contexts >= MaxConcurrentHTTPContexts) {
        // There can only be 8 HTTP contexts open at the same time for any particular session.
        LOG_ERROR(Service_HTTP, "Tried to open too many HTTP contexts");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::TooManyContexts, ErrorModule::HTTP, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }

    if (method == RequestMethod::None || static_cast<u32>(method) >= TotalRequestMethods) {
        LOG_ERROR(Service_HTTP, "invalid request method={}", method);

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::InvalidRequestMethod, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }

    contexts.try_emplace(++context_counter);
    contexts[context_counter].url = std::move(url);
    contexts[context_counter].method = method;
    contexts[context_counter].state = RequestState::NotStarted;
    // TODO(Subv): Find a correct default value for this field.
    contexts[context_counter].socket_buffer_size = 0;
    contexts[context_counter].handle = context_counter;
    contexts[context_counter].session_id = session_data->session_id;

    session_data->num_http_contexts++;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(context_counter);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::CloseContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    ASSERT_MSG(!session_data->current_http_context,
               "Unimplemented CloseContext on context-bound session");

    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        // The real HTTP module just silently fails in this case.
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(RESULT_SUCCESS);
        LOG_ERROR(Service_HTTP, "called, context {} not found", context_handle);
        return;
    }

    // TODO(Subv): What happens if you try to close a context that's currently being used?
    // TODO(Subv): Make sure that only the session that created the context can close it.

    // Note that this will block if a request is still in progress
    contexts.erase(itr);
    session_data->num_http_contexts--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::AddRequestHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    if (itr->second.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::InvalidRequestState, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    ASSERT(std::find_if(itr->second.headers.begin(), itr->second.headers.end(),
                        [&name](const Context::RequestHeader& m) -> bool {
                            return m.name == name;
                        }) == itr->second.headers.end());

    itr->second.headers.emplace_back(name, value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::AddPostDataAscii(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    [[maybe_unused]] const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    if (itr->second.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add post data on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::InvalidRequestState, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    ASSERT(std::find_if(itr->second.post_data.begin(), itr->second.post_data.end(),
                        [&name](const Context::PostData& m) -> bool { return m.name == name; }) ==
           itr->second.post_data.end());

    itr->second.post_data.emplace_back(name, value);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(value_buffer);
}

void HTTP_C::GetResponseStatusCode(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, false);
}

void HTTP_C::GetResponseStatusCodeTimeout(Kernel::HLERequestContext& ctx) {
    GetResponseStatusCodeImpl(ctx, true);
}

void HTTP_C::GetResponseStatusCodeImpl(Kernel::HLERequestContext& ctx, bool timeout) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();
    u64 timeout_nanos = 0;
    if (timeout) {
        timeout_nanos = rp.Pop<u64>();
        LOG_INFO(Service_HTTP, "called, timeout={}", timeout_nanos);
    } else {
        LOG_INFO(Service_HTTP, "called");
    }

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    if (timeout) {
        itr->second.request_future.wait_for(std::chrono::nanoseconds(timeout));
        // TODO (flTobi): Return error on timeout
    } else {
        itr->second.request_future.wait();
    }

    const u32 response_code = itr->second.response.status;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(response_code);
}

void HTTP_C::SetClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 client_cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called with context_handle={} client_cert_handle={}", context_handle,
              client_cert_handle);

    if (!PerformStateChecks(ctx, rp, context_handle)) {
        return;
    }

    auto http_context_itr = contexts.find(context_handle);
    ASSERT(http_context_itr != contexts.end());

    auto cert_context_itr = client_certs.find(client_cert_handle);
    if (cert_context_itr == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "called with wrong client_cert_handle {}", client_cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_WRONG_CERT_HANDLE);
        return;
    }

    if (http_context_itr->second.ssl_config.client_cert_ctx.lock()) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert to a context that already has a client cert");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_CERT_ALREADY_SET);
        return;
    }

    if (http_context_itr->second.state != RequestState::NotStarted) {
        LOG_ERROR(Service_HTTP,
                  "Tried to set a client cert on a context that has already been started.");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrCodes::InvalidRequestState, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        return;
    }

    http_context_itr->second.ssl_config.client_cert_ctx = cert_context_itr->second;
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::GetSSLError(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const u32 context_handle = rp.Pop<u32>();
    const u32 unk = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, context_handle={}, unk={}", context_handle, unk);

    auto http_context_itr = contexts.find(context_handle);
    ASSERT(http_context_itr != contexts.end());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    // Since we create the actual http/ssl context only when the request is submitted we can't check
    // for SSL Errors here. Just submit no error.
    rb.Push<u32>(0);
}

void HTTP_C::OpenClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 cert_size = rp.Pop<u32>();
    u32 key_size = rp.Pop<u32>();
    Kernel::MappedBuffer& cert_buffer = rp.PopMappedBuffer();
    Kernel::MappedBuffer& key_buffer = rp.PopMappedBuffer();

    LOG_DEBUG(Service_HTTP, "called, cert_size {}, key_size {}", cert_size, key_size);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    ResultCode result(RESULT_SUCCESS);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called without Initialize");
        result = ERROR_STATE_ERROR;
    } else if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        result = ERROR_NOT_IMPLEMENTED;
    } else if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        result = ERROR_TOO_MANY_CLIENT_CERTS;
    } else {
        client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
        client_certs[client_certs_counter]->handle = client_certs_counter;
        client_certs[client_certs_counter]->certificate.resize(cert_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->certificate[0], 0, cert_size);
        client_certs[client_certs_counter]->private_key.resize(key_size);
        cert_buffer.Read(&client_certs[client_certs_counter]->private_key[0], 0, key_size);
        client_certs[client_certs_counter]->session_id = session_data->session_id;

        ++session_data->num_client_certs;
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(cert_buffer);
    rb.PushMappedBuffer(key_buffer);
}

void HTTP_C::OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u8 cert_id = rp.Pop<u8>();

    LOG_DEBUG(Service_HTTP, "called, cert_id={} cert_handle={}", cert_id, client_certs_counter);

    auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    if (session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called with a bound context");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_NOT_IMPLEMENTED);
        return;
    }

    if (session_data->num_client_certs >= 2) {
        LOG_ERROR(Service_HTTP, "tried to load more then 2 client certs");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_TOO_MANY_CLIENT_CERTS);
        return;
    }

    constexpr u8 default_cert_id = 0x40;
    if (cert_id != default_cert_id) {
        LOG_ERROR(Service_HTTP, "called with invalid cert_id {}", cert_id);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_WRONG_CERT_ID);
        return;
    }

    if (!ClCertA.init) {
        LOG_ERROR(Service_HTTP, "called but ClCertA is missing");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(static_cast<ResultCode>(-1));
        return;
    }

    const auto& it = std::find_if(client_certs.begin(), client_certs.end(),
                                  [default_cert_id, &session_data](const auto& i) {
                                      return default_cert_id == i.second->cert_id &&
                                             session_data->session_id == i.second->session_id;
                                  });

    if (it != client_certs.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(it->first);

        LOG_DEBUG(Service_HTTP, "called, with an already loaded cert_id={}", cert_id);
        return;
    }

    client_certs[++client_certs_counter] = std::make_shared<ClientCertContext>();
    client_certs[client_certs_counter]->handle = client_certs_counter;
    client_certs[client_certs_counter]->certificate = ClCertA.certificate;
    client_certs[client_certs_counter]->private_key = ClCertA.private_key;
    client_certs[client_certs_counter]->session_id = session_data->session_id;
    ++session_data->num_client_certs;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(client_certs_counter);
}

void HTTP_C::CloseClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    ClientCertContext::Handle cert_handle = rp.Pop<u32>();

    LOG_DEBUG(Service_HTTP, "called, cert_handle={}", cert_handle);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (client_certs.find(cert_handle) == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "Command called with a unkown client cert handle {}", cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(RESULT_SUCCESS);
        return;
    }

    if (client_certs[cert_handle]->session_id != session_data->session_id) {
        LOG_ERROR(Service_HTTP, "called from another main session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(RESULT_SUCCESS);
        return;
    }

    client_certs.erase(cert_handle);
    session_data->num_client_certs--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::Finalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    shared_memory = nullptr;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_HTTP, "(STUBBED) called");
}

void HTTP_C::GetDownloadSizeState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const Context::Handle context_handle = rp.Pop<u32>();

    LOG_INFO(Service_HTTP, "called");

    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return;
    }

    auto itr = contexts.find(context_handle);
    ASSERT(itr != contexts.end());

    // On the real console, the current downloaded progress and the total size of the content gets
    // returned. Since we do not support chunked downloads on the host, always return the content
    // length if the download is complete and 0 otherwise.
    u32 content_length = 0;
    const bool is_complete = itr->second.request_future.wait_for(std::chrono::milliseconds(0)) ==
                             std::future_status::ready;
    if (is_complete) {
        const auto& headers = itr->second.response.headers;
        const auto& it = headers.find("Content-Length");
        if (it != headers.end()) {
            content_length = std::stoi(it->second);
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(content_length);
    rb.Push(content_length);
}

SessionData* HTTP_C::EnsureSessionInitialized(Kernel::HLERequestContext& ctx,
                                              IPC::RequestParser rp) {
    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to make a request on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
        return nullptr;
    }

    return session_data;
}

bool HTTP_C::PerformStateChecks(Kernel::HLERequestContext& ctx, IPC::RequestParser rp,
                                Context::Handle context_handle) {
    const auto* session_data = EnsureSessionInitialized(ctx, rp);
    if (!session_data) {
        return false;
    }

    // This command can only be called with a bound context
    if (!session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Tried to make a request without a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotImplemented, ErrorModule::HTTP,
                           ErrorSummary::Internal, ErrorLevel::Permanent));
        return false;
    }

    if (session_data->current_http_context != context_handle) {
        LOG_ERROR(
            Service_HTTP,
            "Tried to make a request on a mismatched session input context={} session context={}",
            context_handle, *session_data->current_http_context);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
        return false;
    }

    return true;
}

void HTTP_C::DecryptClCertA() {
    static constexpr u32 iv_length = 16;

    FileSys::NCCHArchive archive(0x0004001b00010002, Service::FS::MediaType::NAND);

    std::array<char, 8> exefs_filepath;
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::RomFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = archive.OpenFile(file_path, open_mode);
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA file missing");
        return;
    }

    auto romfs = std::move(file_result).Unwrap();
    std::vector<u8> romfs_buffer(romfs->GetSize());
    romfs->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->Close();

    if (!HW::AES::IsNormalKeyAvailable(HW::AES::KeySlotID::SSLKey)) {
        LOG_ERROR(Service_HTTP, "NormalKey in KeySlot 0x0D missing");
        return;
    }
    HW::AES::AESKey key = HW::AES::GetNormalKey(HW::AES::KeySlotID::SSLKey);

    const RomFS::RomFSFile cert_file =
        RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-cert.bin"});
    if (cert_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin missing");
        return;
    }
    if (cert_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-cert.bin size is too small. Size: {}",
                  cert_file.Length());
        return;
    }

    std::vector<u8> cert_data(cert_file.Length() - iv_length);

    using CryptoPP::AES;
    CryptoPP::CBC_Mode<AES>::Decryption aes_cert;
    std::array<u8, iv_length> cert_iv;
    std::memcpy(cert_iv.data(), cert_file.Data(), iv_length);
    aes_cert.SetKeyWithIV(key.data(), AES::BLOCKSIZE, cert_iv.data());
    aes_cert.ProcessData(cert_data.data(), cert_file.Data() + iv_length,
                         cert_file.Length() - iv_length);

    const RomFS::RomFSFile key_file =
        RomFS::GetFile(romfs_buffer.data(), {u"ctr-common-1-key.bin"});
    if (key_file.Length() == 0) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin missing");
        return;
    }
    if (key_file.Length() <= iv_length) {
        LOG_ERROR(Service_HTTP, "ctr-common-1-key.bin size is too small. Size: {}",
                  key_file.Length());
        return;
    }

    std::vector<u8> key_data(key_file.Length() - iv_length);

    CryptoPP::CBC_Mode<AES>::Decryption aes_key;
    std::array<u8, iv_length> key_iv;
    std::memcpy(key_iv.data(), key_file.Data(), iv_length);
    aes_key.SetKeyWithIV(key.data(), AES::BLOCKSIZE, key_iv.data());
    aes_key.ProcessData(key_data.data(), key_file.Data() + iv_length,
                        key_file.Length() - iv_length);

    ClCertA.certificate = std::move(cert_data);
    ClCertA.private_key = std::move(key_data);
    ClCertA.init = true;
}

HTTP_C::HTTP_C() : ServiceFramework("http:C", 32) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, &HTTP_C::Initialize, "Initialize"},
        {0x0002, &HTTP_C::CreateContext, "CreateContext"},
        {0x0003, &HTTP_C::CloseContext, "CloseContext"},
        {0x0004, nullptr, "CancelConnection"},
        {0x0005, nullptr, "GetRequestState"},
        {0x0006, &HTTP_C::GetDownloadSizeState, "GetDownloadSizeState"},
        {0x0007, nullptr, "GetRequestError"},
        {0x0008, &HTTP_C::InitializeConnectionSession, "InitializeConnectionSession"},
        {0x0009, &HTTP_C::BeginRequest, "BeginRequest"},
        {0x000A, &HTTP_C::BeginRequestAsync, "BeginRequestAsync"},
        {0x000B, &HTTP_C::ReceiveData, "ReceiveData"},
        {0x000C, &HTTP_C::ReceiveDataTimeout, "ReceiveDataTimeout"},
        {0x000D, nullptr, "SetProxy"},
        {0x000E, nullptr, "SetProxyDefault"},
        {0x000F, nullptr, "SetBasicAuthorization"},
        {0x0010, nullptr, "SetSocketBufferSize"},
        {0x0011, &HTTP_C::AddRequestHeader, "AddRequestHeader"},
        {0x0012, &HTTP_C::AddPostDataAscii, "AddPostDataAscii"},
        {0x0013, nullptr, "AddPostDataBinary"},
        {0x0014, nullptr, "AddPostDataRaw"},
        {0x0015, nullptr, "SetPostDataType"},
        {0x0016, nullptr, "SendPostDataAscii"},
        {0x0017, nullptr, "SendPostDataAsciiTimeout"},
        {0x0018, nullptr, "SendPostDataBinary"},
        {0x0019, nullptr, "SendPostDataBinaryTimeout"},
        {0x001A, nullptr, "SendPostDataRaw"},
        {0x001B, nullptr, "SendPOSTDataRawTimeout"},
        {0x001C, nullptr, "SetPostDataEncoding"},
        {0x001D, nullptr, "NotifyFinishSendPostData"},
        {0x001E, nullptr, "GetResponseHeader"},
        {0x001F, nullptr, "GetResponseHeaderTimeout"},
        {0x0020, nullptr, "GetResponseData"},
        {0x0021, nullptr, "GetResponseDataTimeout"},
        {0x0022, &HTTP_C::GetResponseStatusCode, "GetResponseStatusCode"},
        {0x0023, &HTTP_C::GetResponseStatusCodeTimeout, "GetResponseStatusCodeTimeout"},
        {0x0024, nullptr, "AddTrustedRootCA"},
        {0x0025, nullptr, "AddDefaultCert"},
        {0x0026, nullptr, "SelectRootCertChain"},
        {0x0027, nullptr, "SetClientCert"},
        {0x0029, &HTTP_C::SetClientCertContext, "SetClientCertContext"},
        {0x002A, &HTTP_C::GetSSLError, "GetSSLError"},
        {0x002B, nullptr, "SetSSLOpt"},
        {0x002C, nullptr, "SetSSLClearOpt"},
        {0x002D, nullptr, "CreateRootCertChain"},
        {0x002E, nullptr, "DestroyRootCertChain"},
        {0x002F, nullptr, "RootCertChainAddCert"},
        {0x0030, nullptr, "RootCertChainAddDefaultCert"},
        {0x0031, nullptr, "RootCertChainRemoveCert"},
        {0x0032, &HTTP_C::OpenClientCertContext, "OpenClientCertContext"},
        {0x0033, &HTTP_C::OpenDefaultClientCertContext, "OpenDefaultClientCertContext"},
        {0x0034, &HTTP_C::CloseClientCertContext, "CloseClientCertContext"},
        {0x0035, nullptr, "SetDefaultProxy"},
        {0x0036, nullptr, "ClearDNSCache"},
        {0x0037, nullptr, "SetKeepAlive"},
        {0x0038, nullptr, "SetPostDataTypeSize"},
        {0x0039, &HTTP_C::Finalize, "Finalize"},
        // clang-format on
    };
    RegisterHandlers(functions);

    DecryptClCertA();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace Service::HTTP
