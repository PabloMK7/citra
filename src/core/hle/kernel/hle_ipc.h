// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <boost/container/small_vector.hpp>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/server_session.h"

namespace Service {
class ServiceFrameworkBase;
}

namespace Kernel {

class HandleTable;
class Process;
class Thread;
class Event;

/**
 * Interface implemented by HLE Session handlers.
 * This can be provided to a ServerSession in order to hook into several relevant events
 * (such as a new connection or a SyncRequest) so they can be implemented in the emulator.
 */
class SessionRequestHandler : public std::enable_shared_from_this<SessionRequestHandler> {
public:
    virtual ~SessionRequestHandler() = default;

    /**
     * Handles a sync request from the emulated application.
     * @param server_session The ServerSession that was triggered for this sync request,
     * it should be used to differentiate which client (As in ClientSession) we're answering to.
     * TODO(Subv): Use a wrapper structure to hold all the information relevant to
     * this request (ServerSession, Originator thread, Translated command buffer, etc).
     * @returns ResultCode the result code of the translate operation.
     */
    virtual void HandleSyncRequest(SharedPtr<ServerSession> server_session) = 0;

    /**
     * Signals that a client has just connected to this HLE handler and keeps the
     * associated ServerSession alive for the duration of the connection.
     * @param server_session Owning pointer to the ServerSession associated with the connection.
     */
    virtual void ClientConnected(SharedPtr<ServerSession> server_session);

    /**
     * Signals that a client has just disconnected from this HLE handler and releases the
     * associated ServerSession.
     * @param server_session ServerSession associated with the connection.
     */
    virtual void ClientDisconnected(SharedPtr<ServerSession> server_session);

    /// Empty placeholder structure for services with no per-session data. The session data classes
    /// in each service must inherit from this.
    struct SessionDataBase {
        virtual ~SessionDataBase() = default;
    };

protected:
    /// Creates the storage for the session data of the service.
    virtual std::unique_ptr<SessionDataBase> MakeSessionData() const = 0;

    /// Returns the session data associated with the server session.
    template <typename T>
    T* GetSessionData(SharedPtr<ServerSession> session) {
        static_assert(std::is_base_of<SessionDataBase, T>(),
                      "T is not a subclass of SessionDataBase");
        auto itr = std::find_if(connected_sessions.begin(), connected_sessions.end(),
                                [&](const SessionInfo& info) { return info.session == session; });
        ASSERT(itr != connected_sessions.end());
        return static_cast<T*>(itr->data.get());
    }

    struct SessionInfo {
        SessionInfo(SharedPtr<ServerSession> session, std::unique_ptr<SessionDataBase> data);

        SharedPtr<ServerSession> session;
        std::unique_ptr<SessionDataBase> data;
    };
    /// List of sessions that are connected to this handler. A ServerSession whose server endpoint
    /// is an HLE implementation is kept alive by this list for the duration of the connection.
    std::vector<SessionInfo> connected_sessions;
};

class MappedBuffer {
public:
    MappedBuffer(const Process& process, u32 descriptor, VAddr address, u32 id);

    // interface for service
    void Read(void* dest_buffer, std::size_t offset, std::size_t size);
    void Write(const void* src_buffer, std::size_t offset, std::size_t size);
    std::size_t GetSize() const {
        return size;
    }

    // interface for ipc helper
    u32 GenerateDescriptor() const {
        return IPC::MappedBufferDesc(size, perms);
    }

    u32 GetId() const {
        return id;
    }

private:
    friend class HLERequestContext;
    u32 id;
    VAddr address;
    const Process* process;
    std::size_t size;
    IPC::MappedBufferPermissions perms;
};

/**
 * Class containing information about an in-flight IPC request being handled by an HLE service
 * implementation. Services should avoid using old global APIs (e.g. Kernel::GetCommandBuffer()) and
 * when possible use the APIs in this class to service the request.
 *
 * HLE handle protocol
 * ===================
 *
 * To avoid needing HLE services to keep a separate handle table, or having to directly modify the
 * requester's table, a tweaked protocol is used to receive and send handles in requests. The kernel
 * will decode the incoming handles into object pointers and insert a id in the buffer where the
 * handle would normally be. The service then calls GetIncomingHandle() with that id to get the
 * pointer to the object. Similarly, instead of inserting a handle into the command buffer, the
 * service calls AddOutgoingHandle() and stores the returned id where the handle would normally go.
 *
 * The end result is similar to just giving services their own real handle tables, but since these
 * ids are local to a specific context, it avoids requiring services to manage handles for objects
 * across multiple calls and ensuring that unneeded handles are cleaned up.
 *
 * HLE mapped buffer protocol
 * ==========================
 *
 * HLE services don't have their own virtual memory space, a tweaked protocol is used to simulate
 * memory mapping. The kernel will wrap the incoming buffers into a memory interface on which HLE
 * services can operate, and insert a id in the buffer where the vaddr would normally be. The
 * service then calls GetMappedBuffer with that id to get the memory interface. On response, like
 * real services pushing back the mapped buffer address to unmap it, HLE services push back the
 * id of the memory interface and let kernel convert it back to client vaddr. No real unmapping is
 * needed in this case, though.
 */
class HLERequestContext {
public:
    HLERequestContext(SharedPtr<ServerSession> session);
    ~HLERequestContext();

    /// Returns a pointer to the IPC command buffer for this request.
    u32* CommandBuffer() {
        return cmd_buf.data();
    }

    /**
     * Returns the session through which this request was made. This can be used as a map key to
     * access per-client data on services.
     */
    SharedPtr<ServerSession> Session() const {
        return session;
    }

    using WakeupCallback = std::function<void(SharedPtr<Thread> thread, HLERequestContext& context,
                                              ThreadWakeupReason reason)>;

    /**
     * Puts the specified guest thread to sleep until the returned event is signaled or until the
     * specified timeout expires.
     * @param thread Thread to be put to sleep.
     * @param reason Reason for pausing the thread, to be used for debugging purposes.
     * @param timeout Timeout in nanoseconds after which the thread will be awoken and the callback
     * invoked with a Timeout reason.
     * @param callback Callback to be invoked when the thread is resumed. This callback must write
     * the entire command response once again, regardless of the state of it before this function
     * was called.
     * @returns Event that when signaled will resume the thread and call the callback function.
     */
    SharedPtr<Event> SleepClientThread(SharedPtr<Thread> thread, const std::string& reason,
                                       std::chrono::nanoseconds timeout, WakeupCallback&& callback);

    /**
     * Resolves a object id from the request command buffer into a pointer to an object. See the
     * "HLE handle protocol" section in the class documentation for more details.
     */
    SharedPtr<Object> GetIncomingHandle(u32 id_from_cmdbuf) const;

    /**
     * Adds an outgoing object to the response, returning the id which should be used to reference
     * it. See the "HLE handle protocol" section in the class documentation for more details.
     */
    u32 AddOutgoingHandle(SharedPtr<Object> object);

    /**
     * Discards all Objects from the context, invalidating all ids. This may be called after reading
     * out all incoming objects, so that the buffer memory can be re-used for outgoing handles, but
     * this is not required.
     */
    void ClearIncomingObjects();

    /**
     * Retrieves the static buffer identified by the input buffer_id. The static buffer *must* have
     * been created in PopulateFromIncomingCommandBuffer by way of an input StaticBuffer descriptor.
     */
    const std::vector<u8>& GetStaticBuffer(u8 buffer_id) const;

    /**
     * Sets up a static buffer that will be copied to the target process when the request is
     * translated.
     */
    void AddStaticBuffer(u8 buffer_id, std::vector<u8> data);

    /**
     * Gets a memory interface by the id from the request command buffer. See the "HLE mapped buffer
     * protocol" section in the class documentation for more details.
     */
    MappedBuffer& GetMappedBuffer(u32 id_from_cmdbuf);

    /// Populates this context with data from the requesting process/thread.
    ResultCode PopulateFromIncomingCommandBuffer(const u32_le* src_cmdbuf, Process& src_process,
                                                 HandleTable& src_table);
    /// Writes data from this context back to the requesting process/thread.
    ResultCode WriteToOutgoingCommandBuffer(u32_le* dst_cmdbuf, Process& dst_process,
                                            HandleTable& dst_table) const;

private:
    std::array<u32, IPC::COMMAND_BUFFER_LENGTH> cmd_buf;
    SharedPtr<ServerSession> session;
    // TODO(yuriks): Check common usage of this and optimize size accordingly
    boost::container::small_vector<SharedPtr<Object>, 8> request_handles;
    // The static buffers will be created when the IPC request is translated.
    std::array<std::vector<u8>, IPC::MAX_STATIC_BUFFERS> static_buffers;
    // The mapped buffers will be created when the IPC request is translated
    boost::container::small_vector<MappedBuffer, 8> request_mapped_buffers;
};

} // namespace Kernel
