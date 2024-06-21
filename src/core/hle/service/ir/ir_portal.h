// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <span>
#include <boost/serialization/array.hpp>
#include <queue>
#include "common/bit_field.h"
#include "common/file_util.h"
#include "common/swap.h"
#include "core/frontend/input.h"
#include "core/hle/service/ir/ir_user.h"

constexpr u8 MAX_SKYLANDERS = 16;

namespace Core {
struct TimingEventType;
class Timing;
class Movie;
} // namespace Core

namespace Service::IR {

/**
 * An IRDevice emulating Circle Pad Pro or New 3DS additional HID hardware.
 * This device sends periodic udates at a rate configured by the 3DS, and sends calibration data if
 * requested.
 */
class IRPortal final : public IRDevice {
public:
    explicit IRPortal(SendFunc send_func);
    ~IRPortal();

    void OnConnect() override;
    void OnDisconnect() override;
    void OnReceive(std::span<const u8> data) override;

private:
    void HandlePortalCommand(std::span<const u8> request);

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        if (Archive::is_loading::value) {
        }
    }
    friend class boost::serialization::access;
};

struct Skylander final {
    FileUtil::IOFile sky_file;
    u8 status = 0;
    std::queue<u8> queued_status;
    std::array<u8, 0x40 * 0x10> data{};
    u32 last_id = 0;

    void Save();

    enum : u8 { REMOVED = 0, READY = 1, REMOVING = 2, ADDED = 3 };
};

struct SkylanderLEDColor final {
    u8 red = 0;
    u8 green = 0;
    u8 blue = 0;
};

class SkylanderPortal final {
public:
    void Activate();
    void Deactivate();
    bool IsActivated();
    void SetLEDs(u8 side, u8 r, u8 g, u8 b);

    std::array<u8, 64> GetStatus();
    void QueryBlock(u8 sky_num, u8 block, u8* reply_buf);
    void WriteBlock(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf);

    bool RemoveSkylander(u8 sky_num);
    u8 LoadSkylander(u8* data, FileUtil::IOFile sky_file);

private:
    static bool IsSkylanderNumberValid(u8 sky_num);
    static bool IsBlockNumberValid(u8 block);

    std::mutex sky_mutex;

    bool m_activated = false;
    u8 m_interrupt_counter = 0;
    SkylanderLEDColor m_color_right = {};
    SkylanderLEDColor m_color_left = {};
    SkylanderLEDColor m_color_trap = {};

    std::array<Skylander, MAX_SKYLANDERS> skylanders;
};

extern SkylanderPortal g_skyportal;

} // namespace Service::IR

BOOST_CLASS_EXPORT_KEY(Service::IR::IRPortal)
