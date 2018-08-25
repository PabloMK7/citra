// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <future>
#include "common/logging/log.h"
#include "web_service/announce_room_json.h"
#include "web_service/json.h"
#include "web_service/web_backend.h"

namespace AnnounceMultiplayerRoom {

void to_json(nlohmann::json& json, const Room::Member& member) {
    json["name"] = member.name;
    json["gameName"] = member.game_name;
    json["gameId"] = member.game_id;
}

void from_json(const nlohmann::json& json, Room::Member& member) {
    member.name = json.at("name").get<std::string>();
    member.game_name = json.at("gameName").get<std::string>();
    member.game_id = json.at("gameId").get<u64>();
}

void to_json(nlohmann::json& json, const Room& room) {
    json["id"] = room.UID;
    json["port"] = room.port;
    json["name"] = room.name;
    json["preferredGameName"] = room.preferred_game;
    json["preferredGameId"] = room.preferred_game_id;
    json["maxPlayers"] = room.max_player;
    json["netVersion"] = room.net_version;
    json["hasPassword"] = room.has_password;
    if (room.members.size() > 0) {
        nlohmann::json member_json = room.members;
        json["players"] = member_json;
    }
}

void from_json(const nlohmann::json& json, Room& room) {
    room.ip = json.at("address").get<std::string>();
    room.name = json.at("name").get<std::string>();
    room.owner = json.at("owner").get<std::string>();
    room.port = json.at("port").get<u16>();
    room.preferred_game = json.at("preferredGameName").get<std::string>();
    room.preferred_game_id = json.at("preferredGameId").get<u64>();
    room.max_player = json.at("maxPlayers").get<u32>();
    room.net_version = json.at("netVersion").get<u32>();
    room.has_password = json.at("hasPassword").get<bool>();
    try {
        room.members = json.at("players").get<std::vector<Room::Member>>();
    } catch (const nlohmann::detail::out_of_range& e) {
        LOG_DEBUG(Network, "Out of range {}", e.what());
    }
}

} // namespace AnnounceMultiplayerRoom

namespace WebService {

void RoomJson::SetRoomInformation(const std::string& uid, const std::string& name, const u16 port,
                                  const u32 max_player, const u32 net_version,
                                  const bool has_password, const std::string& preferred_game,
                                  const u64 preferred_game_id) {
    room.name = name;
    room.UID = uid;
    room.port = port;
    room.max_player = max_player;
    room.net_version = net_version;
    room.has_password = has_password;
    room.preferred_game = preferred_game;
    room.preferred_game_id = preferred_game_id;
}
void RoomJson::AddPlayer(const std::string& nickname,
                         const AnnounceMultiplayerRoom::MacAddress& mac_address, const u64 game_id,
                         const std::string& game_name) {
    AnnounceMultiplayerRoom::Room::Member member;
    member.name = nickname;
    member.mac_address = mac_address;
    member.game_id = game_id;
    member.game_name = game_name;
    room.members.push_back(member);
}

std::future<Common::WebResult> RoomJson::Announce() {
    nlohmann::json json = room;
    return PostJson(endpoint_url, json.dump(), false);
}

void RoomJson::ClearPlayers() {
    room.members.clear();
}

std::future<AnnounceMultiplayerRoom::RoomList> RoomJson::GetRoomList(std::function<void()> func) {
    auto DeSerialize = [func](const std::string& reply) -> AnnounceMultiplayerRoom::RoomList {
        nlohmann::json json = nlohmann::json::parse(reply);
        AnnounceMultiplayerRoom::RoomList room_list =
            json.at("rooms").get<AnnounceMultiplayerRoom::RoomList>();
        func();
        return room_list;
    };
    return GetJson<AnnounceMultiplayerRoom::RoomList>(DeSerialize, endpoint_url, true);
}

void RoomJson::Delete() {
    nlohmann::json json;
    json["id"] = room.UID;
    DeleteJson(endpoint_url, json.dump());
}

} // namespace WebService
