// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ssl_c.h"

SERIALIZE_EXPORT_IMPL(Service::SSL::SSL_C)
namespace Service::SSL {

void SSL_C::Initialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 0, 2);
    rp.PopPID();

    // Seed random number generator when the SSL service is initialized
    std::random_device rand_device;
    rand_gen.seed(rand_device());

    // Stub, return success
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void SSL_C::GenerateRandomData(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 1, 2);
    u32 size = rp.Pop<u32>();
    auto buffer = rp.PopMappedBuffer();

    // Fill the output buffer with random data.
    u32 data = 0;
    u32 i = 0;
    while (i < size) {
        if ((i % 4) == 0) {
            // The random number generator returns 4 bytes worth of data, so generate new random
            // data when i == 0 and when i is divisible by 4
            data = rand_gen();
        }

        if (size > 4) {
            // Use up the entire 4 bytes of the random data for as long as possible
            buffer.Write(&data, i, 4);
            i += 4;
        } else if (size == 2) {
            buffer.Write(&data, i, 2);
            i += 2;
        } else {
            buffer.Write(&data, i, 1);
            i++;
        }
    }

    // Stub, return success
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMappedBuffer(buffer);
}

SSL_C::SSL_C() : ServiceFramework("ssl:C") {
    static const FunctionInfo functions[] = {
        {0x00010002, &SSL_C::Initialize, "Initialize"},
        {0x000200C2, nullptr, "CreateContext"},
        {0x00030000, nullptr, "CreateRootCertChain"},
        {0x00040040, nullptr, "DestroyRootCertChain"},
        {0x00050082, nullptr, "AddTrustedRootCA"},
        {0x00060080, nullptr, "RootCertChainAddDefaultCert"},
        {0x00070080, nullptr, "RootCertChainRemoveCert"},
        {0x000D0084, nullptr, "OpenClientCertContext"},
        {0x000E0040, nullptr, "OpenDefaultClientCertContext"},
        {0x000F0040, nullptr, "CloseClientCertContext"},
        {0x00110042, &SSL_C::GenerateRandomData, "GenerateRandomData"},
        {0x00120042, nullptr, "InitializeConnectionSession"},
        {0x00130040, nullptr, "StartConnection"},
        {0x00140040, nullptr, "StartConnectionGetOut"},
        {0x00150082, nullptr, "Read"},
        {0x00160082, nullptr, "ReadPeek"},
        {0x00170082, nullptr, "Write"},
        {0x00180080, nullptr, "ContextSetRootCertChain"},
        {0x00190080, nullptr, "ContextSetClientCert"},
        {0x001B0080, nullptr, "ContextClearOpt"},
        {0x001C00C4, nullptr, "ContextGetProtocolCipher"},
        {0x001E0040, nullptr, "DestroyContext"},
        {0x001F0082, nullptr, "ContextInitSharedmem"},
    };

    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<SSL_C>()->InstallAsService(service_manager);
}

} // namespace Service::SSL
