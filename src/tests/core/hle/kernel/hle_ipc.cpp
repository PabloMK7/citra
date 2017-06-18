// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch.hpp>
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

static SharedPtr<Object> MakeObject() {
    return Event::Create(ResetType::OneShot);
}

TEST_CASE("HLERequestContext::PopoulateFromIncomingCommandBuffer", "[core][kernel]") {
    auto session = std::get<SharedPtr<ServerSession>>(ServerSession::CreateSessionPair());
    HLERequestContext context(std::move(session));

    auto process = Process::Create(CodeSet::Create("", 0));
    HandleTable handle_table;

    SECTION("works with empty cmdbuf") {
        const u32_le input[]{
            IPC::MakeHeader(0x1234, 0, 0),
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        REQUIRE(context.CommandBuffer()[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        const u32_le input[]{
            IPC::MakeHeader(0, 3, 0), 0x12345678, 0x21122112, 0xAABBCCDD,
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        auto* output = context.CommandBuffer();
        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move handles") {
        auto a = MakeObject();
        Handle a_handle = handle_table.Create(a).Unwrap();
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2), IPC::MoveHandleDesc(1), a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(handle_table.GetGeneric(a_handle) == nullptr);
    }

    SECTION("translates copy handles") {
        auto a = MakeObject();
        Handle a_handle = handle_table.Create(a).Unwrap();
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2), IPC::CopyHandleDesc(1), a_handle,
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(handle_table.GetGeneric(a_handle) == a);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject();
        auto b = MakeObject();
        auto c = MakeObject();
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 5),        IPC::MoveHandleDesc(2),
            handle_table.Create(a).Unwrap(), handle_table.Create(b).Unwrap(),
            IPC::MoveHandleDesc(1),          handle_table.Create(c).Unwrap(),
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        auto* output = context.CommandBuffer();
        REQUIRE(context.GetIncomingHandle(output[2]) == a);
        REQUIRE(context.GetIncomingHandle(output[3]) == b);
        REQUIRE(context.GetIncomingHandle(output[5]) == c);
    }

    SECTION("translates CallingPid descriptors") {
        const u32_le input[]{
            IPC::MakeHeader(0, 0, 2), IPC::CallingPidDesc(), 0x98989898,
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        REQUIRE(context.CommandBuffer()[2] == process->process_id);
    }

    SECTION("translates mixed params") {
        auto a = MakeObject();
        const u32_le input[]{
            IPC::MakeHeader(0, 2, 4),
            0x12345678,
            0xABCDEF00,
            IPC::MoveHandleDesc(1),
            handle_table.Create(a).Unwrap(),
            IPC::CallingPidDesc(),
            0,
        };

        context.PopulateFromIncomingCommandBuffer(input, *process, handle_table);

        auto* output = context.CommandBuffer();
        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0xABCDEF00);
        REQUIRE(context.GetIncomingHandle(output[4]) == a);
        REQUIRE(output[6] == process->process_id);
    }
}

TEST_CASE("HLERequestContext::WriteToOutgoingCommandBuffer", "[core][kernel]") {
    auto session = std::get<SharedPtr<ServerSession>>(ServerSession::CreateSessionPair());
    HLERequestContext context(std::move(session));

    auto process = Process::Create(CodeSet::Create("", 0));
    HandleTable handle_table;
    auto* input = context.CommandBuffer();
    u32_le output[IPC::COMMAND_BUFFER_LENGTH];

    SECTION("works with empty cmdbuf") {
        input[0] = IPC::MakeHeader(0x1234, 0, 0);

        context.WriteToOutgoingCommandBuffer(output, *process, handle_table);

        REQUIRE(output[0] == 0x12340000);
    }

    SECTION("translates regular params") {
        input[0] = IPC::MakeHeader(0, 3, 0);
        input[1] = 0x12345678;
        input[2] = 0x21122112;
        input[3] = 0xAABBCCDD;

        context.WriteToOutgoingCommandBuffer(output, *process, handle_table);

        REQUIRE(output[1] == 0x12345678);
        REQUIRE(output[2] == 0x21122112);
        REQUIRE(output[3] == 0xAABBCCDD);
    }

    SECTION("translates move/copy handles") {
        auto a = MakeObject();
        auto b = MakeObject();
        input[0] = IPC::MakeHeader(0, 0, 4);
        input[1] = IPC::MoveHandleDesc(1);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = IPC::CopyHandleDesc(1);
        input[4] = context.AddOutgoingHandle(b);

        context.WriteToOutgoingCommandBuffer(output, *process, handle_table);

        REQUIRE(handle_table.GetGeneric(output[2]) == a);
        REQUIRE(handle_table.GetGeneric(output[4]) == b);
    }

    SECTION("translates multi-handle descriptors") {
        auto a = MakeObject();
        auto b = MakeObject();
        auto c = MakeObject();
        input[0] = IPC::MakeHeader(0, 0, 5);
        input[1] = IPC::MoveHandleDesc(2);
        input[2] = context.AddOutgoingHandle(a);
        input[3] = context.AddOutgoingHandle(b);
        input[4] = IPC::CopyHandleDesc(1);
        input[5] = context.AddOutgoingHandle(c);

        context.WriteToOutgoingCommandBuffer(output, *process, handle_table);

        REQUIRE(handle_table.GetGeneric(output[2]) == a);
        REQUIRE(handle_table.GetGeneric(output[3]) == b);
        REQUIRE(handle_table.GetGeneric(output[5]) == c);
    }
}

} // namespace Kernel
