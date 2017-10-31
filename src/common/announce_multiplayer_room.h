// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <future>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Common {
struct WebResult {
    enum Code : u32 {
        Success,
        InvalidURL,
        CredentialsMissing,
        CprError,
        HttpError,
        WrongContent,
        NoWebservice,
    };
    Code result_code;
    std::string result_string;
};
} // namespace Common

namespace AnnounceMultiplayerRoom {

using MacAddress = std::array<u8, 6>;

struct Room {
    struct Member {
        std::string name;
        MacAddress mac_address;
        std::string game_name;
        u64 game_id;
    };
    std::string name;
    std::string GUID;
    std::string owner;
    std::string ip;
    u16 port;
    u32 max_player;
    u32 net_version;
    bool has_password;
    std::string preferred_game;
    u64 preferred_game_id;

    std::vector<Member> members;
};
using RoomList = std::vector<Room>;

/**
 * A AnnounceMultiplayerRoom interface class. A backend to submit/get to/from a web service should
 * implement this interface.
 */
class Backend : NonCopyable {
public:
    virtual ~Backend() = default;
    virtual void SetRoomInformation(const std::string& guid, const std::string& name,
                                    const u16 port, const u32 max_player, const u32 net_version,
                                    const bool has_password, const std::string& preferred_game,
                                    const u64 preferred_game_id) = 0;
    virtual void AddPlayer(const std::string& nickname, const MacAddress& mac_address,
                           const u64 game_id, const std::string& game_name) = 0;
    virtual std::future<Common::WebResult> Announce() = 0;
    virtual void ClearPlayers() = 0;
    virtual std::future<RoomList> GetRoomList(std::function<void()> func) = 0;
    virtual void Delete() = 0;
};

/**
 * Empty implementation of AnnounceMultiplayerRoom interface that drops all data. Used when a
 * functional backend implementation is not available.
 */
class NullBackend : public Backend {
public:
    ~NullBackend() = default;
    void SetRoomInformation(const std::string& /*guid*/, const std::string& /*name*/,
                            const u16 /*port*/, const u32 /*max_player*/, const u32 /*net_version*/,
                            const bool /*has_password*/, const std::string& /*preferred_game*/,
                            const u64 /*preferred_game_id*/) override {}
    void AddPlayer(const std::string& /*nickname*/, const MacAddress& /*mac_address*/,
                   const u64 /*game_id*/, const std::string& /*game_name*/) override {}
    std::future<Common::WebResult> Announce() override {
        return std::async(std::launch::async, []() {
            return Common::WebResult{Common::WebResult::Code::NoWebservice,
                                     "WebService is missing"};
        });
    }
    void ClearPlayers() override {}
    std::future<RoomList> GetRoomList(std::function<void()> func) override {
        return std::async(std::launch::async, [func]() {
            func();
            return RoomList{};
        });
    }

    void Delete() override {}
};

} // namespace AnnounceMultiplayerRoom
