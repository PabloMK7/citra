// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <thread>
#include <libusb.h>
#include "common/logging/log.h"
#include "input_common/gcadapter/gc_adapter.h"

namespace GCAdapter {

/// Used to loop through and assign button in poller
constexpr std::array<PadButton, 12> PadButtonArray{
    PadButton::PAD_BUTTON_LEFT, PadButton::PAD_BUTTON_RIGHT, PadButton::PAD_BUTTON_DOWN,
    PadButton::PAD_BUTTON_UP,   PadButton::PAD_TRIGGER_Z,    PadButton::PAD_TRIGGER_R,
    PadButton::PAD_TRIGGER_L,   PadButton::PAD_BUTTON_A,     PadButton::PAD_BUTTON_B,
    PadButton::PAD_BUTTON_X,    PadButton::PAD_BUTTON_Y,     PadButton::PAD_BUTTON_START,
};

Adapter::Adapter() {
    if (usb_adapter_handle != nullptr) {
        return;
    }
    LOG_INFO(Input, "GC Adapter Initialization started");

    const int init_res = libusb_init(&libusb_ctx);
    if (init_res == LIBUSB_SUCCESS) {
        Setup();
    } else {
        LOG_ERROR(Input, "libusb could not be initialized. failed with error = {}", init_res);
    }
}

GCPadStatus Adapter::GetPadStatus(std::size_t port, const std::array<u8, 37>& adapter_payload) {
    GCPadStatus pad = {};
    const std::size_t offset = 1 + (9 * port);

    adapter_controllers_status[port] = static_cast<ControllerTypes>(adapter_payload[offset] >> 4);

    static constexpr std::array<PadButton, 8> b1_buttons{
        PadButton::PAD_BUTTON_A,    PadButton::PAD_BUTTON_B,    PadButton::PAD_BUTTON_X,
        PadButton::PAD_BUTTON_Y,    PadButton::PAD_BUTTON_LEFT, PadButton::PAD_BUTTON_RIGHT,
        PadButton::PAD_BUTTON_DOWN, PadButton::PAD_BUTTON_UP,
    };

    static constexpr std::array<PadButton, 4> b2_buttons{
        PadButton::PAD_BUTTON_START,
        PadButton::PAD_TRIGGER_Z,
        PadButton::PAD_TRIGGER_R,
        PadButton::PAD_TRIGGER_L,
    };

    static constexpr std::array<PadAxes, 6> axes{
        PadAxes::StickX,    PadAxes::StickY,      PadAxes::SubstickX,
        PadAxes::SubstickY, PadAxes::TriggerLeft, PadAxes::TriggerRight,
    };

    if (adapter_controllers_status[port] == ControllerTypes::None && !get_origin[port]) {
        // Controller may have been disconnected, recalibrate if reconnected.
        get_origin[port] = true;
    }

    if (adapter_controllers_status[port] != ControllerTypes::None) {
        const u8 b1 = adapter_payload[offset + 1];
        const u8 b2 = adapter_payload[offset + 2];

        for (std::size_t i = 0; i < b1_buttons.size(); ++i) {
            if ((b1 & (1U << i)) != 0) {
                pad.button |= static_cast<u16>(b1_buttons[i]);
            }
        }

        for (std::size_t j = 0; j < b2_buttons.size(); ++j) {
            if ((b2 & (1U << j)) != 0) {
                pad.button |= static_cast<u16>(b2_buttons[j]);
            }
        }
        for (PadAxes axis : axes) {
            const std::size_t index = static_cast<std::size_t>(axis);
            pad.axis_values[index] = adapter_payload[offset + 3 + index];
        }

        if (get_origin[port]) {
            origin_status[port].axis_values = pad.axis_values;
            get_origin[port] = false;
        }
    }
    return pad;
}

void Adapter::PadToState(const GCPadStatus& pad, GCState& state) {
    for (const auto& button : PadButtonArray) {
        const u16 button_value = static_cast<u16>(button);
        state.buttons.insert_or_assign(button_value, pad.button & button_value);
    }

    for (size_t i = 0; i < pad.axis_values.size(); ++i) {
        state.axes.insert_or_assign(static_cast<u8>(i), pad.axis_values[i]);
    }
}

void Adapter::Read() {
    LOG_DEBUG(Input, "GC Adapter Read() thread started");

    int payload_size;
    std::array<u8, 37> adapter_payload;
    std::array<GCPadStatus, 4> pads;

    while (adapter_thread_running) {
        libusb_interrupt_transfer(usb_adapter_handle, input_endpoint, adapter_payload.data(),
                                  sizeof(adapter_payload), &payload_size, 16);

        if (payload_size != sizeof(adapter_payload) || adapter_payload[0] != LIBUSB_DT_HID) {
            LOG_ERROR(Input,
                      "Error reading payload (size: {}, type: {:02x}) Is the adapter connected?",
                      payload_size, adapter_payload[0]);
            adapter_thread_running = false; // error reading from adapter, stop reading.
            break;
        }
        for (std::size_t port = 0; port < pads.size(); ++port) {
            pads[port] = GetPadStatus(port, adapter_payload);
            if (DeviceConnected(port) && configuring) {
                if (pads[port].button != 0) {
                    pad_queue[port].Push(pads[port]);
                }

                // Accounting for a threshold here to ensure an intentional press
                for (size_t i = 0; i < pads[port].axis_values.size(); ++i) {
                    const u8 value = pads[port].axis_values[i];
                    const u8 origin = origin_status[port].axis_values[i];

                    if (value > origin + pads[port].THRESHOLD ||
                        value < origin - pads[port].THRESHOLD) {
                        pads[port].axis = static_cast<PadAxes>(i);
                        pads[port].axis_value = pads[port].axis_values[i];
                        pad_queue[port].Push(pads[port]);
                    }
                }
            }
            PadToState(pads[port], state[port]);
        }
        std::this_thread::yield();
    }
}

void Adapter::Setup() {
    // Initialize all controllers as unplugged
    adapter_controllers_status.fill(ControllerTypes::None);
    // Initialize all ports to store axis origin values
    get_origin.fill(true);

    // pointer to list of connected usb devices
    libusb_device** devices{};

    // populate the list of devices, get the count
    const ssize_t device_count = libusb_get_device_list(libusb_ctx, &devices);
    if (device_count < 0) {
        LOG_ERROR(Input, "libusb_get_device_list failed with error: {}", device_count);
        return;
    }

    if (devices != nullptr) {
        for (std::size_t index = 0; index < static_cast<std::size_t>(device_count); ++index) {
            if (CheckDeviceAccess(devices[index])) {
                // GC Adapter found and accessible, registering it
                GetGCEndpoint(devices[index]);
                break;
            }
        }
        libusb_free_device_list(devices, 1);
    }
}

bool Adapter::CheckDeviceAccess(libusb_device* device) {
    libusb_device_descriptor desc;
    const int get_descriptor_error = libusb_get_device_descriptor(device, &desc);
    if (get_descriptor_error) {
        // could not acquire the descriptor, no point in trying to use it.
        LOG_ERROR(Input, "libusb_get_device_descriptor failed with error: {}",
                  get_descriptor_error);
        return false;
    }

    if (desc.idVendor != 0x057e || desc.idProduct != 0x0337) {
        // This isn't the device we are looking for.
        return false;
    }
    const int open_error = libusb_open(device, &usb_adapter_handle);

    if (open_error == LIBUSB_ERROR_ACCESS) {
        LOG_ERROR(Input, "Yuzu can not gain access to this device: ID {:04X}:{:04X}.",
                  desc.idVendor, desc.idProduct);
        return false;
    }
    if (open_error) {
        LOG_ERROR(Input, "libusb_open failed to open device with error = {}", open_error);
        return false;
    }

    int kernel_driver_error = libusb_kernel_driver_active(usb_adapter_handle, 0);
    if (kernel_driver_error == 1) {
        kernel_driver_error = libusb_detach_kernel_driver(usb_adapter_handle, 0);
        if (kernel_driver_error != 0 && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = {}",
                      kernel_driver_error);
        }
    }

    if (kernel_driver_error && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    const int interface_claim_error = libusb_claim_interface(usb_adapter_handle, 0);
    if (interface_claim_error) {
        LOG_ERROR(Input, "libusb_claim_interface failed with error = {}", interface_claim_error);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    return true;
}

void Adapter::GetGCEndpoint(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    const int config_descriptor_return = libusb_get_config_descriptor(device, 0, &config);
    if (config_descriptor_return != LIBUSB_SUCCESS) {
        LOG_ERROR(Input, "libusb_get_config_descriptor failed with error = {}",
                  config_descriptor_return);
        return;
    }

    for (u8 ic = 0; ic < config->bNumInterfaces; ic++) {
        const libusb_interface* interfaceContainer = &config->interface[ic];
        for (int i = 0; i < interfaceContainer->num_altsetting; i++) {
            const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
            for (u8 e = 0; e < interface->bNumEndpoints; e++) {
                const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                    input_endpoint = endpoint->bEndpointAddress;
                } else {
                    output_endpoint = endpoint->bEndpointAddress;
                }
            }
        }
    }
    // This transfer seems to be responsible for clearing the state of the adapter
    // Used to clear the "busy" state of when the device is unexpectedly unplugged
    unsigned char clear_payload = 0x13;
    libusb_interrupt_transfer(usb_adapter_handle, output_endpoint, &clear_payload,
                              sizeof(clear_payload), nullptr, 16);

    adapter_thread_running = true;
    adapter_input_thread = std::thread(&Adapter::Read, this);
}

Adapter::~Adapter() {
    Reset();
}

void Adapter::Reset() {
    if (adapter_thread_running) {
        adapter_thread_running = false;
    }
    if (adapter_input_thread.joinable()) {
        adapter_input_thread.join();
    }

    adapter_controllers_status.fill(ControllerTypes::None);
    get_origin.fill(true);

    if (usb_adapter_handle) {
        libusb_release_interface(usb_adapter_handle, 1);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
    }

    if (libusb_ctx) {
        libusb_exit(libusb_ctx);
    }
}

bool Adapter::DeviceConnected(std::size_t port) {
    return adapter_controllers_status[port] != ControllerTypes::None;
}

void Adapter::ResetDeviceType(std::size_t port) {
    adapter_controllers_status[port] = ControllerTypes::None;
}

void Adapter::BeginConfiguration() {
    get_origin.fill(true);
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = true;
}

void Adapter::EndConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = false;
}

std::array<Common::SPSCQueue<GCPadStatus>, 4>& Adapter::GetPadQueue() {
    return pad_queue;
}

const std::array<Common::SPSCQueue<GCPadStatus>, 4>& Adapter::GetPadQueue() const {
    return pad_queue;
}

std::array<GCState, 4>& Adapter::GetPadState() {
    return state;
}

const std::array<GCState, 4>& Adapter::GetPadState() const {
    return state;
}

int Adapter::GetOriginValue(int port, int axis) const {
    return origin_status[port].axis_values[axis];
}

} // namespace GCAdapter
