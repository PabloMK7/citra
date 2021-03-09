// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"

struct libusb_context;
struct libusb_device;
struct libusb_device_handle;

namespace GCAdapter {

enum class PadButton {
    PAD_BUTTON_LEFT = 0x0001,
    PAD_BUTTON_RIGHT = 0x0002,
    PAD_BUTTON_DOWN = 0x0004,
    PAD_BUTTON_UP = 0x0008,
    PAD_TRIGGER_Z = 0x0010,
    PAD_TRIGGER_R = 0x0020,
    PAD_TRIGGER_L = 0x0040,
    PAD_BUTTON_A = 0x0100,
    PAD_BUTTON_B = 0x0200,
    PAD_BUTTON_X = 0x0400,
    PAD_BUTTON_Y = 0x0800,
    PAD_BUTTON_START = 0x1000,
    // Below is for compatibility with "AxisButton" type
    PAD_STICK = 0x2000,
};

extern const std::array<PadButton, 12> PadButtonArray;

enum class PadAxes : u8 {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    TriggerLeft,
    TriggerRight,
    Undefined,
};

struct GCPadStatus {
    u16 button{}; // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits

    std::array<u8, 6> axis_values{};    // Triggers and sticks, following indices defined in PadAxes
    static constexpr u8 THRESHOLD = 50; // Threshold for axis press for polling

    u8 port{};
    PadAxes axis{PadAxes::Undefined};
    u8 axis_value{255};
};

struct GCState {
    std::unordered_map<int, bool> buttons;
    std::unordered_map<int, u16> axes;
};

enum class ControllerTypes { None, Wired, Wireless };

class Adapter {
public:
    /// Initialize the GC Adapter capture and read sequence
    Adapter();

    /// Close the adapter read thread and release the adapter
    ~Adapter();
    /// Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    /// Returns true if there is a device connected to port
    bool DeviceConnected(std::size_t port);

    std::array<Common::SPSCQueue<GCPadStatus>, 4>& GetPadQueue();
    const std::array<Common::SPSCQueue<GCPadStatus>, 4>& GetPadQueue() const;

    std::array<GCState, 4>& GetPadState();
    const std::array<GCState, 4>& GetPadState() const;

    int GetOriginValue(int port, int axis) const;

private:
    GCPadStatus GetPadStatus(std::size_t port, const std::array<u8, 37>& adapter_payload);

    void PadToState(const GCPadStatus& pad, GCState& state);

    void Read();

    /// Resets status of device connected to port
    void ResetDeviceType(std::size_t port);

    /// Returns true if we successfully gain access to GC Adapter
    bool CheckDeviceAccess(libusb_device* device);

    /// Captures GC Adapter endpoint address,
    void GetGCEndpoint(libusb_device* device);

    /// For shutting down, clear all data, join all threads, release usb
    void Reset();

    /// For use in initialization, querying devices to find the adapter
    void Setup();

    libusb_device_handle* usb_adapter_handle = nullptr;

    std::thread adapter_input_thread;
    bool adapter_thread_running;

    libusb_context* libusb_ctx;

    u8 input_endpoint = 0;
    u8 output_endpoint = 0;

    bool configuring = false;

    std::array<GCState, 4> state;
    std::array<bool, 4> get_origin;
    std::array<GCPadStatus, 4> origin_status;
    std::array<Common::SPSCQueue<GCPadStatus>, 4> pad_queue;
    std::array<ControllerTypes, 4> adapter_controllers_status{};
};

} // namespace GCAdapter
