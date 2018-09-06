// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/romfs.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/http_c.h"
#include "core/hw/aes/key.h"

namespace Service {
namespace HTTP {

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
};
}

const ResultCode ERROR_STATE_ERROR = // 0xD8A0A066
    ResultCode(ErrCodes::SessionStateError, ErrorModule::HTTP, ErrorSummary::InvalidState,
               ErrorLevel::Permanent);

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
    if (session_data->current_http_context != boost::none) {
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

    ASSERT_MSG(session_data->current_http_context == boost::none,
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
    if (session_data->current_http_context == boost::none) {
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
                  context_handle, session_data->current_http_context.get());
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

void HTTP_C::DecryptClCertA() {
    static constexpr u32 iv_length = 16;

    FileSys::Path archive_path =
        FileSys::MakeNCCHArchivePath(0x0004001b00010002, Service::FS::MediaType::NAND);
    auto archive_result = Service::FS::OpenArchive(Service::FS::ArchiveIdCode::NCCH, archive_path);
    if (archive_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA archive missing");
        return;
    }

    std::array<char, 8> exefs_filepath;
    FileSys::Path file_path = FileSys::MakeNCCHFilePath(
        FileSys::NCCHFileOpenType::NCCHData, 0, FileSys::NCCHFilePathType::RomFS, exefs_filepath);
    FileSys::Mode open_mode = {};
    open_mode.read_flag.Assign(1);
    auto file_result = Service::FS::OpenFileFromArchive(*archive_result, file_path, open_mode);
    if (file_result.Failed()) {
        LOG_ERROR(Service_HTTP, "ClCertA file missing");
        return;
    }

    auto romfs = std::move(file_result).Unwrap();
    std::vector<u8> romfs_buffer(romfs->backend->GetSize());
    romfs->backend->Read(0, romfs_buffer.size(), romfs_buffer.data());
    romfs->backend->Close();

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

    DecryptClCertA();
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<HTTP_C>()->InstallAsService(service_manager);
}
} // namespace HTTP
} // namespace Service
