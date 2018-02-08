// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <future>
#include <string>
#include "common/announce_multiplayer_room.h"

namespace WebService {

/**
 * Implementation of AnnounceMultiplayerRoom::Backend that (de)serializes room information into/from
 * JSON, and submits/gets it to/from the Citra web service
 */
class RoomJson : public AnnounceMultiplayerRoom::Backend {
public:
    RoomJson(const std::string& endpoint_url, const std::string& username, const std::string& token)
        : endpoint_url(endpoint_url), username(username), token(token) {}
    ~RoomJson() = default;
    void SetRoomInformation(const std::string& uid, const std::string& name, const u16 port,
                            const u32 max_player, const u32 net_version, const bool has_password,
                            const std::string& preferred_game,
                            const u64 preferred_game_id) override;
    void AddPlayer(const std::string& nickname,
                   const AnnounceMultiplayerRoom::MacAddress& mac_address, const u64 game_id,
                   const std::string& game_name) override;
    std::future<Common::WebResult> Announce() override;
    void ClearPlayers() override;
    std::future<AnnounceMultiplayerRoom::RoomList> GetRoomList(std::function<void()> func) override;
    void Delete() override;

private:
    AnnounceMultiplayerRoom::Room room;
    std::string endpoint_url;
    std::string username;
    std::string token;
};

} // namespace WebService
