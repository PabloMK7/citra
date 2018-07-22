// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/service/http_c.h"

namespace Service {
namespace HTTP {

namespace ErrCodes {
enum {
    InvalidRequestMethod = 32,
    InvalidContext = 102,
};
}

const ResultCode ERROR_CONTEXT_ERROR = // 0xD8A0A066
    ResultCode(ErrCodes::InvalidContext, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);

enum class RequestMethod : u8 {
    None = 0x0,
    Get = 0x1,
    Post = 0x2,
    Head = 0x3,
    Put = 0x4,
    Delete = 0x5,
    PostEmpty = 0x6,
    PutEmpty = 0x7,
};

/// The number of request methods, any valid method must be less than this.
constexpr u32 TotalRequestMethods = 8;

enum class RequestState : u8 {
    NotStarted = 0x1,             // Request has not started yet.
    InProgress = 0x5,             // Request in progress, sending request over the network.
    ReadyToDownloadContent = 0x7, // Ready to download the content. (needs verification)
    ReadyToDownload = 0x8,        // Ready to download?
    TimedOut = 0xA,               // Request timed out?
};

/// Represents a client certificate along with its private key, stored as a byte array of DER data.
/// There can only be at most one client certificate context attached to an HTTP context at any
/// given time.
struct ClientCertContext {
    u32 handle;
    std::vector<u8> certificate;
    std::vector<u8> private_key;
};

/// Represents a root certificate chain, it contains a list of DER-encoded certificates for
/// verifying HTTP requests. An HTTP context can have at most one root certificate chain attached to
/// it, but the chain may contain an arbitrary number of certificates in it.
struct RootCertChain {
    struct RootCACert {
        u32 handle;
        std::vector<u8> certificate;
    };

    u32 handle;
    std::vector<RootCACert> certificates;
};

/// Represents an HTTP context.
struct Context {
    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
        u16 port;
    };

    struct BasicAuth {
        std::string username;
        std::string password;
    };

    struct RequestHeader {
        std::string name;
        std::string value;
    };

    struct PostData {
        // TODO(Subv): Support Binary and Raw POST elements.
        std::string name;
        std::string value;
    };

    struct SSLConfig {
        u32 options;
        std::weak_ptr<ClientCertContext> client_cert_ctx;
        std::weak_ptr<RootCertChain> root_ca_chain;
    };

    u32 handle;
    std::string url;
    RequestMethod method;
    RequestState state = RequestState::NotStarted;
    boost::optional<Proxy> proxy;
    boost::optional<BasicAuth> basic_auth;
    SSLConfig ssl_config{};
    u32 socket_buffer_size;
    std::vector<RequestHeader> headers;
    std::vector<PostData> post_data;
};

void HTTP_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1, 1, 4);
    const u32 shmem_size = rp.Pop<u32>();
    rp.PopPID();
    shared_memory = rp.PopObject<Kernel::SharedMemory>();
    if (shared_memory) {
        shared_memory->name = "HTTP_C:shared_memory";
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    // This returns 0xd8a0a046 if no network connection is available.
    // Just assume we are always connected.
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_HTTP, "(STUBBED) called, shared memory size: {}", shmem_size);
}

void HTTP_C::CreateContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x2, 2, 2);
    const u32 url_size = rp.Pop<u32>();
    std::string url(url_size, '\0');
    RequestMethod method = rp.PopEnum<RequestMethod>();

    Kernel::MappedBuffer& buffer = rp.PopMappedBuffer();
    buffer.Read(&url[0], 0, url_size - 1);

    LOG_DEBUG(Service_HTTP, "called, url_size={}, url={}, method={}", url_size, url,
              static_cast<u32>(method));

    if (method == RequestMethod::None || static_cast<u32>(method) >= TotalRequestMethods) {
        LOG_ERROR(Service_HTTP, "invalid request method={}", static_cast<u32>(method));

        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(ResultCode(ErrCodes::InvalidContext, ErrorModule::HTTP, ErrorSummary::InvalidState,
                           ErrorLevel::Permanent));
        rb.PushMappedBuffer(buffer);
        return;
    }

    Context context{};
    context.url = std::move(url);
    context.method = method;
    context.state = RequestState::NotStarted;
    // TODO(Subv): Find a correct default value for this field.
    context.socket_buffer_size = 0;
    context.handle = ++context_counter;
    contexts[context_counter] = std::move(context);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 2);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(context_counter);
    rb.PushMappedBuffer(buffer);
}

void HTTP_C::CloseContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3, 2, 0);

    u32 context_handle = rp.Pop<u32>();

    LOG_WARNING(Service_HTTP, "(STUBBED) called, handle={}", context_handle);

    auto itr = contexts.find(context_handle);
    if (itr == contexts.end()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERROR_CONTEXT_ERROR);
        LOG_ERROR(Service_HTTP, "called, context {} not found", context_handle);
        return;
    }

    // TODO(Subv): What happens if you try to close a context that's currently being used?
    ASSERT(itr->second.state == RequestState::NotStarted);

    contexts.erase(itr);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
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
        {0x00080042, nullptr, "InitializeConnectionSession"},
        {0x00090040, nullptr, "BeginRequest"},
        {0x000A0040, nullptr, "BeginRequestAsync"},
        {0x000B0082, nullptr, "ReceiveData"},
        {0x000C0102, nullptr, "ReceiveDataTimeout"},
        {0x000D0146, nullptr, "SetProxy"},
        {0x000E0040, nullptr, "SetProxyDefault"},
        {0x000F00C4, nullptr, "SetBasicAuthorization"},
        {0x00100080, nullptr, "SetSocketBufferSize"},
        {0x001100C4, nullptr, "AddRequestHeader"},
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
        {0x00320084, nullptr, "OpenClientCertContext"},
        {0x00330040, nullptr, "OpenDefaultClientCertContext"},
        {0x00340040, nullptr, "CloseClientCertContext"},
        {0x00350186, nullptr, "SetDefaultProxy"},
        {0x00360000, nullptr, "ClearDNSCache"},
        {0x00370080, nullptr, "SetKeepAlive"},
        {0x003800C0, nullptr, "SetPostDataTypeSize"},
        {0x00390000, nullptr, "Finalize"},
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace HTTP
} // namespace Service
