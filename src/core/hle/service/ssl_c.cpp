// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <random>

#include "core/hle/hle.h"
#include "core/hle/service/ssl_c.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SSL_C

namespace SSL_C {

// TODO: Implement a proper CSPRNG in the future when actual security is needed
static std::mt19937 rand_gen;

static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // Seed random number generator when the SSL service is initialized
    std::random_device rand_device;
    rand_gen.seed(rand_device());

    // Stub, return success
    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void GenerateRandomData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 size = cmd_buff[1];
    VAddr address = cmd_buff[3];
    u8* output_buff = Memory::GetPointer(address);

    // Fill the output buffer with random data.
    u32 data = 0;
    u32 i = 0;
    while (i < size) {
        if ((i % 4) == 0) {
            // The random number generator returns 4 bytes worth of data, so generate new random data when i == 0 and when i is divisible by 4
            data = rand_gen();
        }

        if (size > 4) {
            // Use up the entire 4 bytes of the random data for as long as possible
            *(u32*)(output_buff + i) = data;
            i += 4;
        } else if (size == 2) {
            *(u16*)(output_buff + i) = (u16)(data & 0xffff);
            i += 2;
        } else {
            *(u8*)(output_buff + i) = (u8)(data & 0xff);
            i++;
        }
    }

    // Stub, return success
    cmd_buff[1] = RESULT_SUCCESS.raw;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, Initialize,            "Initialize"},
    {0x000200C2, nullptr,               "CreateContext"},
    {0x00030000, nullptr,               "CreateRootCertChain"},
    {0x00040040, nullptr,               "DestroyRootCertChain"},
    {0x00050082, nullptr,               "AddTrustedRootCA"},
    {0x00060080, nullptr,               "RootCertChainAddDefaultCert"},
    {0x00110042, GenerateRandomData,    "GenerateRandomData"},
    {0x00150082, nullptr,               "Read"},
    {0x00170082, nullptr,               "Write"},
    {0x00180080, nullptr,               "ContextSetRootCertChain"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
