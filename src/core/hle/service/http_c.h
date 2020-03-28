// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <future>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <boost/optional.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/shared_ptr.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/weak_ptr.hpp>
#ifdef ENABLE_WEB_SERVICE
#if defined(__ANDROID__)
#include <ifaddrs.h>
#endif
#include <httplib.h>
#endif
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::HTTP {

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
    using Handle = u32;
    Handle handle;
    u32 session_id;
    u8 cert_id;
    std::vector<u8> certificate;
    std::vector<u8> private_key;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& handle;
        ar& session_id;
        ar& cert_id;
        ar& certificate;
        ar& private_key;
    }
    friend class boost::serialization::access;
};

/// Represents a root certificate chain, it contains a list of DER-encoded certificates for
/// verifying HTTP requests. An HTTP context can have at most one root certificate chain attached to
/// it, but the chain may contain an arbitrary number of certificates in it.
struct RootCertChain {
    struct RootCACert {
        using Handle = u32;
        Handle handle;
        u32 session_id;
        std::vector<u8> certificate;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& handle;
            ar& session_id;
            ar& certificate;
        }
        friend class boost::serialization::access;
    };

    using Handle = u32;
    Handle handle;
    u32 session_id;
    std::vector<RootCACert> certificates;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& handle;
        ar& session_id;
        ar& certificates;
    }
    friend class boost::serialization::access;
};

/// Represents an HTTP context.
class Context final {
public:
    using Handle = u32;

    Context() = default;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

    void MakeRequest();

    struct Proxy {
        std::string url;
        std::string username;
        std::string password;
        u16 port;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& url;
            ar& username;
            ar& password;
            ar& port;
        }
        friend class boost::serialization::access;
    };

    struct BasicAuth {
        std::string username;
        std::string password;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& username;
            ar& password;
        }
        friend class boost::serialization::access;
    };

    struct RequestHeader {
        RequestHeader(std::string name, std::string value) : name(name), value(value){};
        std::string name;
        std::string value;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& name;
            ar& value;
        }
        friend class boost::serialization::access;
    };

    struct PostData {
        // TODO(Subv): Support Binary and Raw POST elements.
        PostData(std::string name, std::string value) : name(name), value(value){};
        PostData() = default;
        std::string name;
        std::string value;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& name;
            ar& value;
        }
        friend class boost::serialization::access;
    };

    struct SSLConfig {
        u32 options;
        std::weak_ptr<ClientCertContext> client_cert_ctx;
        std::weak_ptr<RootCertChain> root_ca_chain;

    private:
        template <class Archive>
        void serialize(Archive& ar, const unsigned int) {
            ar& options;
            ar& client_cert_ctx;
            ar& root_ca_chain;
        }
        friend class boost::serialization::access;
    };

    Handle handle;
    u32 session_id;
    std::string url;
    RequestMethod method;
    std::atomic<RequestState> state = RequestState::NotStarted;
    std::optional<Proxy> proxy;
    std::optional<BasicAuth> basic_auth;
    SSLConfig ssl_config{};
    u32 socket_buffer_size;
    std::vector<RequestHeader> headers;
    std::vector<PostData> post_data;

    std::future<void> request_future;
    std::atomic<u64> current_download_size_bytes;
    std::atomic<u64> total_download_size_bytes;
#ifdef ENABLE_WEB_SERVICE
    httplib::Response response;
#endif
};

struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
    /// The HTTP context that is currently bound to this session, this can be empty if no context
    /// has been bound. Certain commands can only be called on a session with a bound context.
    boost::optional<Context::Handle> current_http_context;

    u32 session_id;

    /// Number of HTTP contexts that are currently opened in this session.
    u32 num_http_contexts = 0;
    /// Number of ClientCert contexts that are currently opened in this session.
    u32 num_client_certs = 0;

    /// Whether this session has been initialized in some way, be it via Initialize or
    /// InitializeConnectionSession.
    bool initialized = false;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler::SessionDataBase>(
            *this);
        ar& current_http_context;
        ar& session_id;
        ar& num_http_contexts;
        ar& num_client_certs;
        ar& initialized;
    }
    friend class boost::serialization::access;
};

class HTTP_C final : public ServiceFramework<HTTP_C, SessionData> {
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
     * HTTP_C::BeginRequest service function
     *  Inputs:
     * 1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginRequest(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::BeginRequestAsync service function
     *  Inputs:
     * 1 : Context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void BeginRequestAsync(Kernel::HLERequestContext& ctx);

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

    /**
     * HTTP_C::AddPostDataAscii service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Form name buffer size, including null-terminator.
     * 3 : Form value buffer size, including null-terminator.
     * 4 : (FormNameSize<<14) | 0xC02
     * 5 : Form name data pointer
     * 6 : (FormValueSize<<4) | 10
     * 7 : Form value data pointer
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void AddPostDataAscii(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::SetClientCertContext service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Cert context handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void SetClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::GetSSLError service function
     *  Inputs:
     * 1 : Context handle
     * 2 : Unknown
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : SSL Error code
     */
    void GetSSLError(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::OpenClientCertContext service function
     *  Inputs:
     *      1 :  Cert size
     *      2 :  Key size
     *      3 :  (CertSize<<4) | 10
     *      4 :  Pointer to input cert
     *      5 :  (KeySize<<4) | 10
     *      6 :  Pointer to input key
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void OpenClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::OpenDefaultClientCertContext service function
     *  Inputs:
     * 1 : CertID
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Client Cert context handle
     */
    void OpenDefaultClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::CloseClientCertContext service function
     *  Inputs:
     * 1 : ClientCert Handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void CloseClientCertContext(Kernel::HLERequestContext& ctx);

    /**
     * HTTP_C::Finalize service function
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Finalize(Kernel::HLERequestContext& ctx);

    void DecryptClCertA();

    std::shared_ptr<Kernel::SharedMemory> shared_memory = nullptr;

    /// The next number to use when a new HTTP session is initalized.
    u32 session_counter = 0;

    /// The next handle number to use when a new HTTP context is created.
    Context::Handle context_counter = 0;

    /// The next handle number to use when a new ClientCert context is created.
    ClientCertContext::Handle client_certs_counter = 0;

    /// Global list of HTTP contexts currently opened.
    std::unordered_map<Context::Handle, Context> contexts;

    /// Global list of  ClientCert contexts currently opened.
    std::unordered_map<ClientCertContext::Handle, std::shared_ptr<ClientCertContext>> client_certs;

    struct {
        std::vector<u8> certificate;
        std::vector<u8> private_key;
        bool init = false;
    } ClCertA;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        // NOTE: Serialization of the HTTP service is on a 'best effort' basis.
        // There is a very good chance that saving/loading during a network connection will break,
        // regardless!
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
        ar& ClCertA.certificate;
        ar& ClCertA.private_key;
        ar& ClCertA.init;
        ar& context_counter;
        ar& client_certs_counter;
        ar& client_certs;
        // NOTE: `contexts` is not serialized because it contains non-serializable data. (i.e.
        // handles to ongoing HTTP requests.) Serializing across HTTP contexts will break.
    }
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::HTTP

BOOST_CLASS_EXPORT_KEY(Service::HTTP::HTTP_C)
BOOST_CLASS_EXPORT_KEY(Service::HTTP::SessionData)
