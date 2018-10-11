// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/romfs.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/http_c.h"
#include "core/hw/aes/key.h"

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

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 1, 4);
    const u32 shmem_size = rp.Pop<u32>();
    u32 pid = rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory) {
        shared_memory->name = "HTTP_C:shared_memory";
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
    IPC::RequestParser rp(ctx, 0x8, 1, 2);
    const Context::Handle context_handle = rp.Pop<u32>();
    u32 pid = rp.PopPID();

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
    LOG_DEBUG(Service_HTTP, "called, context_id={} pid={}", context_handle, pid);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2, 2, 2);
    const u32 url_size = rp.Pop<u32>();
    RequestMethod method = rp.PopEnum<RequestMethod>();
    Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();

    // Copy the buffer into a string without the \0 at the end of the buffer
    std::string url(url_size, '\0');
    buffer.Read(&url[0], 0, url_size - 1);

    LOG_DEBUG(Service_HTTP, "called, url_size={}, url={}, method={}", url_size, url,
              static_cast<u32>(method));

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to create a context on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(buffer);
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
        LOG_ERROR(Service_HTTP, "invalid request method={}", static_cast<u32>(method));

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::InvalidRequestMethod, ErrorModule::HTTP,
                           ErrorSummary::InvalidState, ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }

    contexts.emplace(++context_counter, Context());
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
    IPC::RequestParser rp(ctx, 0x3, 1, 0);

    u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to close a context on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
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
    ASSERT(itr->second.state == RequestState::NotStarted);

    // TODO(Subv): Make sure that only the session that created the context can close it.

    contexts.erase(itr);
    session_data->num_http_contexts--;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void HTTP_C::AddRequestHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 3, 4);
    const u32 context_handle = rp.Pop<u32>();
    const u32 name_size = rp.Pop<u32>();
    const u32 value_size = rp.Pop<u32>();
    const std::vector<u8> name_buffer = rp.PopStaticBuffer();
    Kernel::MappedBuffer& value_buffer = rp.PopMappedBuffer();

    // Copy the name_buffer into a string without the \0 at the end
    const std::string name(name_buffer.begin(), name_buffer.end() - 1);

    // Copy the value_buffer into a string without the \0 at the end
    std::string value(value_size - 1, '\0');
    value_buffer.Read(&value[0], 0, value_size - 1);

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Tried to add a request header on an uninitialized session");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    // This command can only be called with a bound context
    if (!session_data->current_http_context) {
        LOG_ERROR(Service_HTTP, "Command called without a bound context");

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrorDescription::NotImplemented, ErrorModule::HTTP,
                           ErrorSummary::Internal, ErrorLevel::Permanent));
        rb.PushMappedBuffer(value_buffer);
        return;
    }

    if (session_data->current_http_context != context_handle) {
        LOG_ERROR(Service_HTTP,
                  "Tried to add a request header on a mismatched session input context={} session "
                  "context={}",
                  context_handle, *session_data->current_http_context);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ERROR_STATE_ERROR);
        rb.PushMappedBuffer(value_buffer);
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

    LOG_DEBUG(Service_HTTP, "called, name={}, value={}, context_handle={}", name, value,
              context_handle);
}

void HTTP_C::OpenClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x32, 2, 4);
    u32 cert_size = rp.Pop<u32>();
    u32 key_size = rp.Pop<u32>();
    Kernel::MappedBuffer& cert_buffer = rp.PopMappedBuffer();
    Kernel::MappedBuffer& key_buffer = rp.PopMappedBuffer();

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
        ++client_certs_counter;
        client_certs[client_certs_counter].handle = client_certs_counter;
        client_certs[client_certs_counter].certificate.resize(cert_size);
        cert_buffer.Read(&client_certs[client_certs_counter].certificate[0], 0, cert_size);
        client_certs[client_certs_counter].private_key.resize(key_size);
        cert_buffer.Read(&client_certs[client_certs_counter].private_key[0], 0, key_size);
        client_certs[client_certs_counter].session_id = session_data->session_id;

        ++session_data->num_client_certs;
    }

    LOG_DEBUG(Service_HTTP, "called, cert_size {}, key_size {}", cert_size, key_size);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(cert_buffer);
    rb.PushMappedBuffer(key_buffer);
}

void HTTP_C::OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x33, 1, 0);
    u8 cert_id = rp.Pop<u8>();

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (!session_data->initialized) {
        LOG_ERROR(Service_HTTP, "Command called without Initialize");
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_STATE_ERROR);
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
                                      return default_cert_id == i.second.cert_id &&
                                             session_data->session_id == i.second.session_id;
                                  });

    if (it != client_certs.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(it->first);

        LOG_DEBUG(Service_HTTP, "called, with an already loaded cert_id={}", cert_id);
        return;
    }

    ++client_certs_counter;
    client_certs[client_certs_counter].handle = client_certs_counter;
    client_certs[client_certs_counter].certificate = ClCertA.certificate;
    client_certs[client_certs_counter].private_key = ClCertA.private_key;
    client_certs[client_certs_counter].session_id = session_data->session_id;
    ++session_data->num_client_certs;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(client_certs_counter);

    LOG_DEBUG(Service_HTTP, "called, cert_id={}", cert_id);
}

void HTTP_C::CloseClientCertContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x34, 1, 0);
    ClientCertContext::Handle cert_handle = rp.Pop<u32>();

    auto* session_data = GetSessionData(ctx.Session());
    ASSERT(session_data);

    if (client_certs.find(cert_handle) == client_certs.end()) {
        LOG_ERROR(Service_HTTP, "Command called with a unkown client cert handle {}", cert_handle);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        // This just return success without doing anything
        rb.Push(RESULT_SUCCESS);
        return;
    }

    if (client_certs[cert_handle].session_id != session_data->session_id) {
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

    LOG_DEBUG(Service_HTTP, "called, cert_handle={}", cert_handle);
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
        {0x00010044, &HTTP_C::Initialize, "Initialize"},
        {0x00020082, &HTTP_C::CreateContext, "CreateContext"},
        {0x00030040, &HTTP_C::CloseContext, "CloseContext"},
        {0x00040040, nullptr, "CancelConnection"},
        {0x00050040, nullptr, "GetRequestState"},
        {0x00060040, nullptr, "GetDownloadSizeState"},
        {0x00070040, nullptr, "GetRequestError"},
        {0x00080042, &HTTP_C::InitializeConnectionSession, "InitializeConnectionSession"},
        {0x00090040, nullptr, "BeginRequest"},
        {0x000A0040, nullptr, "BeginRequestAsync"},
        {0x000B0082, nullptr, "ReceiveData"},
        {0x000C0102, nullptr, "ReceiveDataTimeout"},
        {0x000D0146, nullptr, "SetProxy"},
        {0x000E0040, nullptr, "SetProxyDefault"},
        {0x000F00C4, nullptr, "SetBasicAuthorization"},
        {0x00100080, nullptr, "SetSocketBufferSize"},
        {0x001100C4, &HTTP_C::AddRequestHeader, "AddRequestHeader"},
        {0x001200C4, nullptr, "AddPostDataAscii"},
        {0x001300C4, nullptr, "AddPostDataBinary"},
        {0x00140082, nullptr, "AddPostDataRaw"},
        {0x00150080, nullptr, "SetPostDataType"},
        {0x001600C4, nullptr, "SendPostDataAscii"},
        {0x00170144, nullptr, "SendPostDataAsciiTimeout"},
        {0x001800C4, nullptr, "SendPostDataBinary"},
        {0x00190144, nullptr, "SendPostDataBinaryTimeout"},
        {0x001A0082, nullptr, "SendPostDataRaw"},
        {0x001B0102, nullptr, "SendPOSTDataRawTimeout"},
        {0x001C0080, nullptr, "SetPostDataEncoding"},
        {0x001D0040, nullptr, "NotifyFinishSendPostData"},
        {0x001E00C4, nullptr, "GetResponseHeader"},
        {0x001F0144, nullptr, "GetResponseHeaderTimeout"},
        {0x00200082, nullptr, "GetResponseData"},
        {0x00210102, nullptr, "GetResponseDataTimeout"},
        {0x00220040, nullptr, "GetResponseStatusCode"},
        {0x002300C0, nullptr, "GetResponseStatusCodeTimeout"},
        {0x00240082, nullptr, "AddTrustedRootCA"},
        {0x00250080, nullptr, "AddDefaultCert"},
        {0x00260080, nullptr, "SelectRootCertChain"},
        {0x002700C4, nullptr, "SetClientCert"},
        {0x00290080, nullptr, "SetClientCertContext"},
        {0x002A0040, nullptr, "GetSSLError"},
        {0x002B0080, nullptr, "SetSSLOpt"},
        {0x002C0080, nullptr, "SetSSLClearOpt"},
        {0x002D0000, nullptr, "CreateRootCertChain"},
        {0x002E0040, nullptr, "DestroyRootCertChain"},
        {0x002F0082, nullptr, "RootCertChainAddCert"},
        {0x00300080, nullptr, "RootCertChainAddDefaultCert"},
        {0x00310080, nullptr, "RootCertChainRemoveCert"},
        {0x00320084, &HTTP_C::OpenClientCertContext, "OpenClientCertContext"},
        {0x00330040, &HTTP_C::OpenDefaultClientCertContext, "OpenDefaultClientCertContext"},
        {0x00340040, &HTTP_C::CloseClientCertContext, "CloseClientCertContext"},
        {0x00350186, nullptr, "SetDefaultProxy"},
        {0x00360000, nullptr, "ClearDNSCache"},
        {0x00370080, nullptr, "SetKeepAlive"},
        {0x003800C0, nullptr, "SetPostDataTypeSize"},
        {0x00390000, nullptr, "Finalize"},
    };
    RegisterHandlers(functions);

    DecryptClCertA();
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace Service::HTTP
