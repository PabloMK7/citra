// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Service {
namespace HTTP {

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

class HTTP_C final : public ServiceFramework<HTTP_C> {
public:
    HTTP_C();

private:
    /**
     * HTTP_C::Initialize service function
     *  Inputs:
     *      1 : POST buffer size
     *      2 : 0x20
     *      3 : 0x0 (Filled with process ID by ARM11 Kernel)
     *      4 : 0x0
     *      5 : POST buffer memory block handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CreateContext service function
     *  Inputs:
     *      1 : URL buffer size, including null-terminator
     *      2 : RequestMethod
     *      3 : (URLSize << 4) | 10
     *      4 : URL data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : HTTP context handle
     */
    void CreateContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CreateContext service function
     *  Inputs:
     *      1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CloseContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::InitializeConnectionSession service function
     *  Inputs:
     *      1 : HTTP context handle
     *      2 : 0x20, processID translate-header for the ARM11-kernel
     *      3 : processID set by the ARM11-kernel
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InitializeConnectionSession(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::AddRequestHeader service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Header name buffer size, including null-terminator.
     * 3 : Header value buffer size, including null-terminator.
     * 4 : (HeaderNameSize<<14) | 0xC02
     * 5 : Header name data pointer
     * 6 : (HeaderValueSize<<4) | 10
     * 7 : Header value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddRequestHeader(Kernel::HLERequestContext& ctx);

    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory = nullptr;

    std::unordered_map<u32, Context> contexts;
    u32 context_counter = 0;

    std::unordered_map<u32, ClientCertContext> client_certs;
    u32 client_certs_counter = 0;
};

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace HTTP
} // namespace Service
