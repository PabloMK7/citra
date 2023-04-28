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
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 2), &SSL_C::Initialize, "Initialize"},
        {IPC::MakeHeader(0x0002, 3, 2), nullptr, "CreateContext"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "CreateRootCertChain"},
        {IPC::MakeHeader(0x0004, 1, 0), nullptr, "DestroyRootCertChain"},
        {IPC::MakeHeader(0x0005, 2, 2), nullptr, "AddTrustedRootCA"},
        {IPC::MakeHeader(0x0006, 2, 0), nullptr, "RootCertChainAddDefaultCert"},
        {IPC::MakeHeader(0x0007, 2, 0), nullptr, "RootCertChainRemoveCert"},
        {IPC::MakeHeader(0x000D, 2, 4), nullptr, "OpenClientCertContext"},
        {IPC::MakeHeader(0x000E, 1, 0), nullptr, "OpenDefaultClientCertContext"},
        {IPC::MakeHeader(0x000F, 1, 0), nullptr, "CloseClientCertContext"},
        {IPC::MakeHeader(0x0011, 1, 2), &SSL_C::GenerateRandomData, "GenerateRandomData"},
        {IPC::MakeHeader(0x0012, 1, 2), nullptr, "InitializeConnectionSession"},
        {IPC::MakeHeader(0x0013, 1, 0), nullptr, "StartConnection"},
        {IPC::MakeHeader(0x0014, 1, 0), nullptr, "StartConnectionGetOut"},
        {IPC::MakeHeader(0x0015, 2, 2), nullptr, "Read"},
        {IPC::MakeHeader(0x0016, 2, 2), nullptr, "ReadPeek"},
        {IPC::MakeHeader(0x0017, 2, 2), nullptr, "Write"},
        {IPC::MakeHeader(0x0018, 2, 0), nullptr, "ContextSetRootCertChain"},
        {IPC::MakeHeader(0x0019, 2, 0), nullptr, "ContextSetClientCert"},
        {IPC::MakeHeader(0x001B, 2, 0), nullptr, "ContextClearOpt"},
        {IPC::MakeHeader(0x001C, 3, 4), nullptr, "ContextGetProtocolCipher"},
        {IPC::MakeHeader(0x001E, 1, 0), nullptr, "DestroyContext"},
        {IPC::MakeHeader(0x001F, 2, 2), nullptr, "ContextInitSharedmem"},
        // clang-format on
    };

    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<SSL_C>()->InstallAsService(service_manager);
}

} // namespace Service::SSL
