// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch_test_macros.hpp>
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

static std::shared_ptr<Object> MakeObject(Kernel::KernelSystem& kernel) {
    return kernel.CreateEvent(ResetType::OneShot);
}

TEST_CASE("HLERequestContext::PopulateFromIncomingCommandBuffer", "[core][kernel]") {
    Core::Timing timing(1, 100);
    Core::System system;
    Memory::MemorySystem memory{system};
    Kernel::KernelSystem kernel(
        memory, timing, [] {}, Kernel::MemoryMode::Prod, 1,
        Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy});
    auto [server, client] = kernel.CreateSessionPair();
    HLERequestContext context(kernel, std::move(server), nullptr);

    auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));

    SECTION("works with empty cmdbuf") {
        const u32_le input[]{
            IPC::MakeHeader(0x1234, 0, 0),
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(context.CommandBuffer()[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        const u32_le input[]{
            IPC::MakeHeader(0, 3, 0),
            0x12345678,
            0x21122112,
            0xAABBCCDD,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move handles") {
        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MoveHandleDesc(1),
            a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(a_handle) == nullptr);
    }

    SECTION("translates copy handles") {
        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::CopyHandleDesc(1),
            a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(a_handle) == a);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        auto c = MakeObject(kernel);
        Handle a_handle, b_handle, c_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        process->handle_table.Create(std::addressof(b_handle), b);
        process->handle_table.Create(std::addressof(c_handle), c);
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 5),
            IPC::MoveHandleDesc(2),
            a_handle,
            b_handle,
            IPC::MoveHandleDesc(1),
            c_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(context.GetIncomingHandle(output[3]) == b);
        REQUIRE(context.GetIncomingHandle(output[5]) == c);
    }

    SECTION("translates null handles") {
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MoveHandleDesc(1),
            0,
        };

        auto result = context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(result == ResultSuccess);
        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == nullptr);
    }

    SECTION("translates CallingPid descriptors") {
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::CallingPidDesc(),
            0x98989898,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        REQUIRE(context.CommandBuffer()[2] == process->process_id);
    }

    SECTION("translates StaticBuffer descriptors") {
        auto mem = std::vector<u8>(Memory::CITRA_PAGE_SIZE);
        std::fill(mem.begin(), mem.end(), 0xAB);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, mem.data(), static_cast<u32>(mem.size()), MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::StaticBufferDesc(mem.size(), 0),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        CHECK(context.GetStaticBuffer(0) == mem);

        REQUIRE(process->vm_manager.UnmapRange(target_address, static_cast<u32>(mem.size())) ==
                ResultSuccess);
    }

    SECTION("translates MappedBuffer descriptors") {
        std::vector<u8> mem(Memory::CITRA_PAGE_SIZE);
        std::fill(mem.begin(), mem.end(), 0xCD);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address, mem.data(), static_cast<u32>(mem.size()), MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MappedBufferDesc(mem.size(), IPC::R),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        std::vector<u8> other_buffer(mem.size());
        context.GetMappedBuffer(0).Read(other_buffer.data(), 0, mem.size());

        CHECK(other_buffer == mem);

        REQUIRE(process->vm_manager.UnmapRange(target_address, static_cast<u32>(mem.size())) ==
                ResultSuccess);
    }

    SECTION("translates mixed params") {
        std::vector<u8> buffer_static(Memory::CITRA_PAGE_SIZE);
        std::fill(buffer_static.begin(), buffer_static.end(), 0xCE);

        std::vector<u8> buffer_mapped(Memory::CITRA_PAGE_SIZE);
        std::fill(buffer_mapped.begin(), buffer_mapped.end(), 0xDF);

        VAddr target_address_static = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(
            target_address_static, buffer_static.data(), static_cast<u32>(buffer_static.size()),
            MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        VAddr target_address_mapped = 0x20000000;
        result = process->vm_manager.MapBackingMemory(target_address_mapped, buffer_mapped.data(),
                                                      static_cast<u32>(buffer_mapped.size()),
                                                      MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        auto a = MakeObject(kernel);
        Handle a_handle;
        process->handle_table.Create(std::addressof(a_handle), a);
        const u32_le input[]{
            IPC::MakeHeader(0, 2, 8),
            0x12345678,
            0xABCDEF00,
            IPC::MoveHandleDesc(1),
            a_handle,
            IPC::CallingPidDesc(),
            0,
            IPC::StaticBufferDesc(buffer_static.size(), 0),
            target_address_static,
            IPC::MappedBufferDesc(buffer_mapped.size(), IPC::R),
            target_address_mapped,
        };

        context.PopulateFromIncomingCommandBuffer(input, process);

        auto* output = context.CommandBuffer();
        CHECK(output[1] == 0x12345678);
        CHECK(output[2] == 0xABCDEF00);
        CHECK(context.GetIncomingHandle(output[4]) == a);
        CHECK(output[6] == process->process_id);
        CHECK(context.GetStaticBuffer(0) == buffer_static);
        std::vector<u8> other_buffer(buffer_mapped.size());
        context.GetMappedBuffer(0).Read(other_buffer.data(), 0, buffer_mapped.size());
        CHECK(other_buffer == buffer_mapped);

        REQUIRE(process->vm_manager.UnmapRange(target_address_static,
                                               static_cast<u32>(buffer_static.size())) ==
                ResultSuccess);
        REQUIRE(process->vm_manager.UnmapRange(target_address_mapped,
                                               static_cast<u32>(buffer_mapped.size())) ==
                ResultSuccess);
    }
}

TEST_CASE("HLERequestContext::WriteToOutgoingCommandBuffer", "[core][kernel]") {
    Core::Timing timing(1, 100);
    Core::System system;
    Memory::MemorySystem memory{system};
    Kernel::KernelSystem kernel(
        memory, timing, [] {}, Kernel::MemoryMode::Prod, 1,
        Kernel::New3dsHwCapabilities{false, false, Kernel::New3dsMemoryMode::Legacy});
    auto [server, client] = kernel.CreateSessionPair();
    HLERequestContext context(kernel, std::move(server), nullptr);

    auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
    auto* input = context.CommandBuffer();
    u32_le output[IPC::COMMAND_BUFFER_LENGTH];

    SECTION("works with empty cmdbuf") {
        input[0] = IPC::MakeHeader(0x1234, 0, 0);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(output[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        input[0] = IPC::MakeHeader(0, 3, 0);
        input[1] = 0x12345678;
        input[2] = 0x21122112;
        input[3] = 0xAABBCCDD;

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move/copy handles") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        input[0] = IPC::MakeHeader(0, 0, 4);
        input[1] = IPC::MoveHandleDesc(1);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = IPC::CopyHandleDesc(1);
        input[4] = context.AddOutgoingHandle(b);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(process->handle_table.GetGeneric(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(output[4]) == b);
    }

    SECTION("translates null handles") {
        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::MoveHandleDesc(1);
        input[2] = context.AddOutgoingHandle(nullptr);

        auto result = context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(result == ResultSuccess);
        REQUIRE(output[2] == 0);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject(kernel);
        auto b = MakeObject(kernel);
        auto c = MakeObject(kernel);
        input[0] = IPC::MakeHeader(0, 0, 5);
        input[1] = IPC::MoveHandleDesc(2);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = context.AddOutgoingHandle(b);
        input[4] = IPC::CopyHandleDesc(1);
        input[5] = context.AddOutgoingHandle(c);

        context.WriteToOutgoingCommandBuffer(output, *process);

        REQUIRE(process->handle_table.GetGeneric(output[2]) == a);
        REQUIRE(process->handle_table.GetGeneric(output[3]) == b);
        REQUIRE(process->handle_table.GetGeneric(output[5]) == c);
    }

    SECTION("translates StaticBuffer descriptors") {
        std::vector<u8> input_buffer(Memory::CITRA_PAGE_SIZE);
        std::fill(input_buffer.begin(), input_buffer.end(), 0xAB);

        context.AddStaticBuffer(0, input_buffer);

        std::vector<u8> output_buffer(Memory::CITRA_PAGE_SIZE);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(target_address, output_buffer.data(),
                                                           static_cast<u32>(output_buffer.size()),
                                                           MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::StaticBufferDesc(input_buffer.size(), 0);
        input[2] = target_address;

        // An entire command buffer plus enough space for one static buffer descriptor and its
        // target address
        std::array<u32_le, IPC::COMMAND_BUFFER_LENGTH + 2> output_cmdbuff;
        // Set up the output StaticBuffer
        output_cmdbuff[IPC::COMMAND_BUFFER_LENGTH] = IPC::StaticBufferDesc(output_buffer.size(), 0);
        output_cmdbuff[IPC::COMMAND_BUFFER_LENGTH + 1] = target_address;

        context.WriteToOutgoingCommandBuffer(output_cmdbuff.data(), *process);

        CHECK(output_buffer == input_buffer);
        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(output_buffer.size())) == ResultSuccess);
    }

    SECTION("translates StaticBuffer descriptors") {
        std::vector<u8> input_buffer(Memory::CITRA_PAGE_SIZE);
        std::fill(input_buffer.begin(), input_buffer.end(), 0xAB);

        std::vector<u8> output_buffer(Memory::CITRA_PAGE_SIZE);

        VAddr target_address = 0x10000000;
        auto result = process->vm_manager.MapBackingMemory(target_address, output_buffer.data(),
                                                           static_cast<u32>(output_buffer.size()),
                                                           MemoryState::Private);
        REQUIRE(result.Code() == ResultSuccess);

        const u32_le input_cmdbuff[]{
            IPC::MakeHeader(0, 0, 2),
            IPC::MappedBufferDesc(output_buffer.size(), IPC::W),
            target_address,
        };

        context.PopulateFromIncomingCommandBuffer(input_cmdbuff, process);

        context.GetMappedBuffer(0).Write(input_buffer.data(), 0, input_buffer.size());

        input[0] = IPC::MakeHeader(0, 0, 2);
        input[1] = IPC::MappedBufferDesc(output_buffer.size(), IPC::W);
        input[2] = 0;

        context.WriteToOutgoingCommandBuffer(output, *process);

        CHECK(output[1] == IPC::MappedBufferDesc(output_buffer.size(), IPC::W));
        CHECK(output[2] == target_address);
        CHECK(output_buffer == input_buffer);
        REQUIRE(process->vm_manager.UnmapRange(
                    target_address, static_cast<u32>(output_buffer.size())) == ResultSuccess);
    }
}

} // namespace Kernel
