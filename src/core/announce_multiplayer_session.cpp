// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <vector>
#include "announce_multiplayer_session.h"
#include "common/announce_multiplayer_room.h"
#include "common/assert.h"
#include "core/settings.h"
#include "network/network.h"

#ifdef ENABLE_WEB_SERVICE
#include "web_service/announce_room_json.h"
#endif

namespace Core {

// Time between room is announced to web_service
static constexpr std::chrono::seconds announce_time_interval(15);

AnnounceMultiplayerSession::AnnounceMultiplayerSession() : announce(false) {
#ifdef ENABLE_WEB_SERVICE
    backend = std::make_unique<WebService::RoomJson>(
        Settings::values.announce_multiplayer_room_endpoint_url, Settings::values.citra_username,
        Settings::values.citra_token);
#else
    backend = std::make_unique<AnnounceMultiplayerRoom::NullBackend>();
#endif
}

void AnnounceMultiplayerSession::Start() {
    if (announce_multiplayer_thread) {
        Stop();
    }

    announce_multiplayer_thread =
        std::make_unique<std::thread>(&AnnounceMultiplayerSession::AnnounceMultiplayerLoop, this);
}

void AnnounceMultiplayerSession::Stop() {
    if (!announce)
        return;
    announce = false;
    // Detaching the loop, to not wait for the sleep to finish. The loop thread will finish soon.
    if (announce_multiplayer_thread) {
        cv.notify_all();
        announce_multiplayer_thread->join();
        announce_multiplayer_thread.reset();
        backend->Delete();
    }
}

AnnounceMultiplayerSession::CallbackHandle AnnounceMultiplayerSession::BindErrorCallback(
    std::function<void(const Common::WebResult&)> function) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    auto handle = std::make_shared<std::function<void(const Common::WebResult&)>>(function);
    error_callbacks.insert(handle);
    return handle;
}

void AnnounceMultiplayerSession::UnbindErrorCallback(CallbackHandle handle) {
    std::lock_guard<std::mutex> lock(callback_mutex);
    error_callbacks.erase(handle);
}

AnnounceMultiplayerSession::~AnnounceMultiplayerSession() {
    Stop();
}

void AnnounceMultiplayerSession::AnnounceMultiplayerLoop() {
    announce = true;
    std::future<Common::WebResult> future;
    while (announce) {
        std::unique_lock<std::mutex> lock(cv_m);
        cv.wait_for(lock, announce_time_interval);
        std::shared_ptr<Network::Room> room = Network::GetRoom().lock();
        if (!room) {
            announce = false;
            continue;
        }
        if (room->GetState() != Network::Room::State::Open) {
            announce = false;
            continue;
        }
        Network::RoomInformation room_information = room->GetRoomInformation();
        std::vector<Network::Room::Member> memberlist = room->GetRoomMemberList();
        backend->SetRoomInformation(
            room_information.uid, room_information.name, room_information.port,
            room_information.member_slots, Network::network_version, room->HasPassword(),
            room_information.preferred_game, room_information.preferred_game_id);
        backend->ClearPlayers();
        for (const auto& member : memberlist) {
            backend->AddPlayer(member.nickname, member.mac_address, member.game_info.id,
                               member.game_info.name);
        }
        future = backend->Announce();
        if (future.valid()) {
            Common::WebResult result = future.get();
            if (result.result_code != Common::WebResult::Code::Success) {
                std::lock_guard<std::mutex> lock(callback_mutex);
                for (auto callback : error_callbacks) {
                    (*callback)(result);
                }
            }
        }
    }
}

std::future<AnnounceMultiplayerRoom::RoomList> AnnounceMultiplayerSession::GetRoomList(
    std::function<void()> func) {
    return backend->GetRoomList(func);
}

} // namespace Core
