// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <boost/range/algorithm_ext/erase.hpp>
#include "common/assert.h"
#include "common/common_types.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/server_session.h"

namespace Kernel {

void SessionRequestHandler::ClientConnected(SharedPtr<ServerSession> server_session) {
    server_session->SetHleHandler(shared_from_this());
    connected_sessions.push_back(server_session);
}

void SessionRequestHandler::ClientDisconnected(SharedPtr<ServerSession> server_session) {
    server_session->SetHleHandler(nullptr);
    boost::range::remove_erase(connected_sessions, server_session);
}

HLERequestContext::HLERequestContext(SharedPtr<ServerSession> session)
    : session(std::move(session)) {
    cmd_buf[0] = 0;
}

HLERequestContext::~HLERequestContext() = default;

SharedPtr<Object> HLERequestContext::GetIncomingHandle(u32 id_from_cmdbuf) const {
    ASSERT(id_from_cmdbuf < request_handles.size());
    return request_handles[id_from_cmdbuf];
}

u32 HLERequestContext::AddOutgoingHandle(SharedPtr<Object> object) {
    request_handles.push_back(std::move(object));
    return static_cast<u32>(request_handles.size() - 1);
}

void HLERequestContext::ClearIncomingObjects() {
    request_handles.clear();
}

ResultCode HLERequestContext::PopulateFromIncomingCommandBuffer(const u32_le* src_cmdbuf,
                                                                Process& src_process,
                                                                HandleTable& src_table) {
    IPC::Header header{src_cmdbuf[0]};

    size_t untranslated_size = 1u + header.normal_params_size;
    size_t command_size = untranslated_size + header.translate_params_size;
    ASSERT(command_size <= IPC::COMMAND_BUFFER_LENGTH); // TODO(yuriks): Return error

    std::copy_n(src_cmdbuf, untranslated_size, cmd_buf.begin());

    size_t i = untranslated_size;
    while (i < command_size) {
        u32 descriptor = cmd_buf[i] = src_cmdbuf[i];
        i += 1;

        switch (IPC::GetDescriptorType(descriptor)) {
        case IPC::DescriptorType::CopyHandle:
        case IPC::DescriptorType::MoveHandle: {
            u32 num_handles = IPC::HandleNumberFromDesc(descriptor);
            ASSERT(i + num_handles <= command_size); // TODO(yuriks): Return error
            for (u32 j = 0; j < num_handles; ++j) {
                Handle handle = src_cmdbuf[i];
                SharedPtr<Object> object = nullptr;
                if (handle != 0) {
                    object = src_table.GetGeneric(handle);
                    ASSERT(object != nullptr); // TODO(yuriks): Return error
                    if (descriptor == IPC::DescriptorType::MoveHandle) {
                        src_table.Close(handle);
                    }
                }

                cmd_buf[i++] = AddOutgoingHandle(std::move(object));
            }
            break;
        }
        case IPC::DescriptorType::CallingPid: {
            cmd_buf[i++] = src_process.process_id;
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unsupported handle translation: 0x%08X", descriptor);
        }
    }

    return RESULT_SUCCESS;
}

ResultCode HLERequestContext::WriteToOutgoingCommandBuffer(u32_le* dst_cmdbuf, Process& dst_process,
                                                           HandleTable& dst_table) const {
    IPC::Header header{cmd_buf[0]};

    size_t untranslated_size = 1u + header.normal_params_size;
    size_t command_size = untranslated_size + header.translate_params_size;
    ASSERT(command_size <= IPC::COMMAND_BUFFER_LENGTH);

    std::copy_n(cmd_buf.begin(), untranslated_size, dst_cmdbuf);

    size_t i = untranslated_size;
    while (i < command_size) {
        u32 descriptor = dst_cmdbuf[i] = cmd_buf[i];
        i += 1;

        switch (IPC::GetDescriptorType(descriptor)) {
        case IPC::DescriptorType::CopyHandle:
        case IPC::DescriptorType::MoveHandle: {
            // HLE services don't use handles, so we treat both CopyHandle and MoveHandle equally
            u32 num_handles = IPC::HandleNumberFromDesc(descriptor);
            ASSERT(i + num_handles <= command_size);
            for (u32 j = 0; j < num_handles; ++j) {
                SharedPtr<Object> object = GetIncomingHandle(cmd_buf[i]);
                Handle handle = 0;
                if (object != nullptr) {
                    // TODO(yuriks): Figure out the proper error handling for if this fails
                    handle = dst_table.Create(object).Unwrap();
                }
                dst_cmdbuf[i++] = handle;
            }
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unsupported handle translation: 0x%08X", descriptor);
        }
    }

    return RESULT_SUCCESS;
}

} // namespace Kernel
