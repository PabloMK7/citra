// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include "core/frontend/input.h"
#include "core/hle/service/ir/ir_user.h"

namespace CoreTiming {
struct EventType;
} // namespace CoreTiming

namespace Service {
namespace IR {

/**
 * An IRDevice emulating Circle Pad Pro or New 3DS additional HID hardware.
 * This device sends periodic udates at a rate configured by the 3DS, and sends calibration data if
 * requested.
 */
class ExtraHID final : public IRDevice {
public:
    explicit ExtraHID(SendFunc send_func);
    ~ExtraHID();

    void OnConnect() override;
    void OnDisconnect() override;
    void OnReceive(const std::vector<u8>& data) override;

    /// Requests input devices reload from current settings. Called when the input settings change.
    void RequestInputDevicesReload();

private:
    void SendHIDStatus();
    void HandleConfigureHIDPollingRequest(const std::vector<u8>& request);
    void HandleReadCalibrationDataRequest(const std::vector<u8>& request);
    void LoadInputDevices();

    u8 hid_period;
    CoreTiming::EventType* hid_polling_callback_id;
    std::array<u8, 0x40> calibration_data;
    std::unique_ptr<Input::ButtonDevice> zl;
    std::unique_ptr<Input::ButtonDevice> zr;
    std::unique_ptr<Input::AnalogDevice> c_stick;
    std::atomic<bool> is_device_reload_pending;
};

} // namespace IR
} // namespace Service
