// Copyright 2024 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "common/alignment.h"
#include "common/settings.h"
#include "core/core_timing.h"
#include "core/hle/service/ir/ir_portal.h"
#include "core/movie.h"

namespace Service::IR {

SkylanderPortal g_skyportal;

IRPortal::IRPortal(SendFunc send_func) : IRDevice(send_func) {}

IRPortal::~IRPortal() {
    OnDisconnect();
}

void IRPortal::OnConnect() {}

void IRPortal::OnDisconnect() {}

void IRPortal::OnReceive(std::span<const u8> data) {
    HandlePortalCommand(data);
}

void IRPortal::HandlePortalCommand(std::span<const u8> data) {
    // Data to be queued to be sent back via the Interrupt Transfer (if needed)
    std::array<u8, 64> response = {};

    // The first byte of the Control Request is always a char for Skylanders (offset by 3 for infrared commands)
    switch (data[3]) {
    case 'A': {
        response = {0x41, data[4], 0xFF, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00,    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        if (data[4] == 0x01) {
            g_skyportal.Activate();
        }
        if (data[4] == 0x00) {
            g_skyportal.Deactivate();
        }
        break;
    }
    case 'C': {
        g_skyportal.SetLEDs(0x01, data[4], data[5], data[6]);
        response = g_skyportal.GetStatus();
        break;
    }
    case 'J': {
        response = {data[3]};
        g_skyportal.SetLEDs(data[4], data[5], data[6], data[7]);
        break;
    }
    case 'L': {
        u8 side = data[4];
        if (side == 0x02) {
            side = 0x04;
        }
        g_skyportal.SetLEDs(side, data[5], data[6], data[7]);
        break;
    }
    case 'M': {
        response = {data[3], data[4], 0x00, 0x19};
        break;
    }
    case 'Q': {
        const u8 sky_num = data[4] & 0xF;
        const u8 block = data[5];
        g_skyportal.QueryBlock(sky_num, block, response.data());
        break;
    }
    case 'R': {
        response = {0x52, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        break;
    }
    case 'S': {
        response = g_skyportal.GetStatus();
        break;
    }
    case 'V': {
        response = g_skyportal.GetStatus();
        break;
    }
    case 'W': {
        const u8 sky_num = data[4] & 0xF;
        const u8 block = data[5];
        g_skyportal.WriteBlock(sky_num, block, &data[6], response.data());
        break;
    }
    default:
        LOG_ERROR(Service_IR, "Unhandled Skylander Portal Query: {}", data[3]);
        break;
    }

    Send(response);
}

void Skylander::Save() {
    if (!sky_file) {
        LOG_ERROR(Service_IR, "Tried to save a Skylander but no file was open");
        return;
    }
    sky_file.Seek(0, SEEK_SET);
    sky_file.WriteBytes(data.data(), 0x40 * 0x10);
    sky_file.Close();
}

bool SkylanderPortal::IsActivated() {
    return m_activated;
}

void SkylanderPortal::Activate() {
    std::lock_guard lock(sky_mutex);
    if (m_activated) {
        // If the portal was already active no change is needed
        return;
    }

    // If not we need to advertise change to all the figures present on the portal
    for (auto& s : skylanders) {
        if (s.status & 1) {
            s.queued_status.push(Skylander::ADDED);
            s.queued_status.push(Skylander::READY);
        }
    }

    m_activated = true;
}

void SkylanderPortal::Deactivate() {
    std::lock_guard lock(sky_mutex);

    for (auto& s : skylanders) {
        // check if at the end of the updates there would be a figure on the portal
        if (!s.queued_status.empty()) {
            s.status = s.queued_status.back();
            s.queued_status = std::queue<u8>();
        }

        s.status &= 1;
    }

    m_activated = false;
}

void SkylanderPortal::SetLEDs(u8 side, u8 red, u8 green, u8 blue) {
    std::lock_guard lock(sky_mutex);
    if (side == 0x00) {
        m_color_right.red = red;
        m_color_right.green = green;
        m_color_right.blue = blue;
    } else if (side == 0x01) {
        m_color_right.red = red;
        m_color_right.green = green;
        m_color_right.blue = blue;

        m_color_left.red = red;
        m_color_left.green = green;
        m_color_left.blue = blue;
    } else if (side == 0x02) {
        m_color_left.red = red;
        m_color_left.green = green;
        m_color_left.blue = blue;
    } else if (side == 0x03) {
        m_color_trap.red = red;
        m_color_trap.green = green;
        m_color_trap.blue = blue;
    }
}

std::array<u8, 64> SkylanderPortal::GetStatus() {
    std::lock_guard lock(sky_mutex);

    u32 status = 0;
    u8 active = 0x00;

    if (m_activated) {
        active = 0x01;
    }

    for (int i = MAX_SKYLANDERS - 1; i >= 0; i--) {
        auto& s = skylanders[i];

        if (!s.queued_status.empty()) {
            s.status = s.queued_status.front();
            s.queued_status.pop();
        }
        status <<= 2;
        status |= s.status;
    }

    std::array<u8, 64> response = {0x53,   0x00, 0x00, 0x00, 0x00, m_interrupt_counter++,
                                   active, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00,   0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00,   0x00};
    memcpy(&response[1], &status, sizeof(status));
    return response;
}

void SkylanderPortal::QueryBlock(u8 sky_num, u8 block, u8* reply_buf) {
    if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
        return;

    std::lock_guard lock(sky_mutex);

    const auto& skylander = skylanders[sky_num];

    reply_buf[0] = 'Q';
    reply_buf[2] = block;
    if (skylander.status & Skylander::READY) {
        reply_buf[1] = (0x10 | sky_num);
        memcpy(&reply_buf[3], skylander.data.data() + (block * 0x10), 0x10);
    } else {
        reply_buf[1] = 0x01;
    }
}

void SkylanderPortal::WriteBlock(u8 sky_num, u8 block, const u8* to_write_buf, u8* reply_buf) {
    if (!IsSkylanderNumberValid(sky_num) || !IsBlockNumberValid(block))
        return;

    std::lock_guard lock(sky_mutex);

    auto& skylander = skylanders[sky_num];

    reply_buf[0] = 'W';
    reply_buf[2] = block;

    if (skylander.status & 1) {
        reply_buf[1] = (0x10 | sky_num);
        memcpy(skylander.data.data() + (block * 0x10), to_write_buf, 0x10);
        skylander.Save();
    } else {
        reply_buf[1] = 0x01;
    }
}

bool SkylanderPortal::RemoveSkylander(u8 sky_num) {
    if (!IsSkylanderNumberValid(sky_num))
        return false;

    LOG_DEBUG(Service_IR, "Cleared Skylander from slot {}", sky_num);
    std::lock_guard lock(sky_mutex);
    auto& skylander = skylanders[sky_num];

    skylander.Save();

    if (skylander.status & Skylander::READY) {
        skylander.status = Skylander::REMOVING;
        skylander.queued_status.push(Skylander::REMOVING);
        skylander.queued_status.push(Skylander::REMOVED);
        return true;
    }

    return false;
}

u8 SkylanderPortal::LoadSkylander(u8* buf, FileUtil::IOFile in_file) {
    std::lock_guard lock(sky_mutex);

    u32 sky_serial = 0;
    for (int i = 3; i > -1; i--) {
        sky_serial <<= 8;
        sky_serial |= buf[i];
    }
    u8 found_slot = 0xFF;

    // mimics spot retaining on the portal
    for (u8 i = 0; i < 8; i++) {
        if ((skylanders[i].status & 1) == 0) {
            if (skylanders[i].last_id == sky_serial) {
                found_slot = i;
                break;
            }

            if (i < found_slot) {
                found_slot = i;
            }
        }
    }

    if (found_slot != 0xff) {
        Skylander& thesky = skylanders[found_slot];
        memcpy(thesky.data.data(), buf, thesky.data.size());
        thesky.sky_file = std::move(in_file);
        thesky.status = 3;
        thesky.queued_status.push(3);
        thesky.queued_status.push(1);
        thesky.last_id = sky_serial;
    }

    return found_slot;
}

bool SkylanderPortal::IsSkylanderNumberValid(u8 sky_num) {
    return sky_num < MAX_SKYLANDERS;
}

bool SkylanderPortal::IsBlockNumberValid(u8 block) {
    return block < 64;
}

} // namespace Service::IR
