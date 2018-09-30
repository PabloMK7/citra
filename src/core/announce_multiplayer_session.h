// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include "common/announce_multiplayer_room.h"
#include "common/common_types.h"
#include "common/thread.h"

namespace Core {

/**
 * Instruments AnnounceMultiplayerRoom::Backend.
 * Creates a thread that regularly updates the room information and submits them
 * An async get of room information is also possible
 */
class AnnounceMultiplayerSession : NonCopyable {
public:
    using CallbackHandle = std::shared_ptr<std::function<void(const Common::WebResult&)>>;
    AnnounceMultiplayerSession();
    ~AnnounceMultiplayerSession();

    /**
     * Allows to bind a function that will get called if the announce encounters an error
     * @param function The function that gets called
     * @return A handle that can be used the unbind the function
     */
    CallbackHandle BindErrorCallback(std::function<void(const Common::WebResult&)> function);

    /**
     * Unbind a function from the error callbacks
     * @param handle The handle for the function that should get unbind
     */
    void UnbindErrorCallback(CallbackHandle handle);

    /**
     * Starts the announce of a room to web services
     */
    void Start();

    /**
     * Stops the announce to web services
     */
    void Stop();

    /**
     *  Returns a list of all room information the backend got
     * @param func A function that gets executed when the async get finished, e.g. a signal
     * @return a list of rooms received from the web service
     */
    AnnounceMultiplayerRoom::RoomList GetRoomList();

private:
    Common::Event shutdown_event;
    std::mutex callback_mutex;
    std::set<CallbackHandle> error_callbacks;
    std::unique_ptr<std::thread> announce_multiplayer_thread;

    /// Backend interface that logs fields
    std::unique_ptr<AnnounceMultiplayerRoom::Backend> backend;

    void AnnounceMultiplayerLoop();
};

} // namespace Core
