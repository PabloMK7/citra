// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <thread>
#include "common/announce_multiplayer_room.h"
#include "common/common_types.h"

namespace Core {

/**
 * Instruments AnnounceMultiplayerRoom::Backend.
 * Creates a thread that regularly updates the room information and submits them
 * An async get of room information is also possible
 */
class AnnounceMultiplayerSession : NonCopyable {
public:
    AnnounceMultiplayerSession();
    ~AnnounceMultiplayerSession();

    /**
     * Allows to bind a function that will get called if the announce encounters an error
     * @param function The function that gets called
     * @return A handle that can be used the unbind the function
     */
    std::shared_ptr<std::function<void(const Common::WebResult&)>> BindErrorCallback(
        std::function<void(const Common::WebResult&)> function);

    /**
     * Unbind a function from the error callbacks
     * @param handle The handle for the function that should get unbind
     */
    void UnbindErrorCallback(std::shared_ptr<std::function<void(const Common::WebResult&)>> handle);

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
    std::future<AnnounceMultiplayerRoom::RoomList> GetRoomList(std::function<void()> func);

private:
    std::atomic<bool> announce{false};
    std::atomic<bool> finished{true};
    std::mutex callback_mutex;
    std::set<std::shared_ptr<std::function<void(const Common::WebResult&)>>> error_callbacks;
    std::unique_ptr<std::thread> announce_multiplayer_thread;

    std::unique_ptr<AnnounceMultiplayerRoom::Backend>
        backend; ///< Backend interface that logs fields

    void AnnounceMultiplayerLoop();
};

} // namespace Core
