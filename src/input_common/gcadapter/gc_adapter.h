// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"

struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

namespace Common {
class ParamPackage;
}

namespace GCAdapter {

enum class PadButton {
    Undefined = 0x0000,
    ButtonLeft = 0x0001,
    ButtonRight = 0x0002,
    ButtonDown = 0x0004,
    ButtonUp = 0x0008,
    TriggerZ = 0x0010,
    TriggerR = 0x0020,
    TriggerL = 0x0040,
    ButtonA = 0x0100,
    ButtonB = 0x0200,
    ButtonX = 0x0400,
    ButtonY = 0x0800,
    ButtonStart = 0x1000,
    // Below is for compatibility with "AxisButton" type
    Stick = 0x2000,
};

enum class PadAxes : u8 {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    TriggerLeft,
    TriggerRight,
    Undefined,
};

enum class ControllerTypes {
    None,
    Wired,
    Wireless,
};

struct GCPadStatus {
    std::size_t port{};

    PadButton button{PadButton::Undefined}; // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits

    PadAxes axis{PadAxes::Undefined};
    s16 axis_value{};
    u8 axis_threshold{50};
};

struct GCController {
    ControllerTypes type{};
    u16 buttons{};
    PadButton last_button{};
    std::array<s16, 6> axis_values{};
    std::array<u8, 6> axis_origin{};
};

class Adapter {
public:
    Adapter();
    ~Adapter();

    /// Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    Common::SPSCQueue<GCPadStatus>& GetPadQueue();
    const Common::SPSCQueue<GCPadStatus>& GetPadQueue() const;

    GCController& GetPadState(std::size_t port);
    const GCController& GetPadState(std::size_t port) const;

    /// Returns true if there is a device connected to port
    bool DeviceConnected(std::size_t port) const;

    std::vector<Common::ParamPackage> GetInputDevices() const;

private:
    using AdapterPayload = std::array<u8, 37>;

    void UpdatePadType(std::size_t port, ControllerTypes pad_type);
    void UpdateControllers(const AdapterPayload& adapter_payload);
    void UpdateSettings(std::size_t port);
    void UpdateStateButtons(std::size_t port, u8 b1, u8 b2);
    void UpdateStateAxes(std::size_t port, const AdapterPayload& adapter_payload);

    void AdapterInputThread();

    void AdapterScanThread();

    bool IsPayloadCorrect(const AdapterPayload& adapter_payload, s32 payload_size);

    /// For use in initialization, querying devices to find the adapter
    void Setup();

    /// Resets status of all GC controller devices to a disconnected state
    void ResetDevices();

    /// Resets status of device connected to a disconnected state
    void ResetDevice(std::size_t port);

    /// Returns true if we successfully gain access to GC Adapter
    bool CheckDeviceAccess();

    /// Captures GC Adapter endpoint address
    /// Returns true if the endpoint was set correctly
    bool GetGCEndpoint(libusb_device* device);

    // Join all threads
    void JoinThreads();

    // Release usb handles
    void ClearLibusbHandle();

    libusb_device_handle* usb_adapter_handle = nullptr;
    std::array<GCController, 4> pads;
    Common::SPSCQueue<GCPadStatus> pad_queue;

    std::thread adapter_input_thread;
    std::thread adapter_scan_thread;
    bool adapter_input_thread_running;
    bool adapter_scan_thread_running;
    bool restart_scan_thread;

    libusb_context* libusb_ctx;

    u8 input_endpoint{0};
    u8 output_endpoint{0};
    u8 input_error_counter{0};

    bool configuring{false};
};
} // namespace GCAdapter
