// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <list>
#include <mutex>
#include <utility>
#include "common/assert.h"
#include "common/threadsafe_queue.h"
#include "input_common/gcadapter/gc_adapter.h"
#include "input_common/gcadapter/gc_poller.h"

namespace InputCommon {

class GCButton final : public Input::ButtonDevice {
public:
    explicit GCButton(int port_, int button_, GCAdapter::Adapter* adapter)
        : port(port_), button(button_), gcadapter(adapter) {}

    ~GCButton() override;

    bool GetStatus() const override {
        if (gcadapter->DeviceConnected(port)) {
            return gcadapter->GetPadState()[port].buttons.at(button);
        }
        return false;
    }

private:
    const int port;
    const int button;
    GCAdapter::Adapter* gcadapter;
};

class GCAxisButton final : public Input::ButtonDevice {
public:
    explicit GCAxisButton(int port_, int axis_, float threshold_, bool trigger_if_greater_,
                          GCAdapter::Adapter* adapter)
        : port(port_), axis(axis_), threshold(threshold_), trigger_if_greater(trigger_if_greater_),
          gcadapter(adapter),
          origin_value(static_cast<float>(adapter->GetOriginValue(port_, axis_))) {}

    bool GetStatus() const override {
        if (gcadapter->DeviceConnected(port)) {
            const float current_axis_value = gcadapter->GetPadState()[port].axes.at(axis);
            const float axis_value = (current_axis_value - origin_value) / 128.0f;
            if (trigger_if_greater) {
                // TODO: Might be worthwile to set a slider for the trigger threshold. It is
                // currently always set to 0.5 in configure_input_player.cpp ZL/ZR HandleClick
                return axis_value > threshold;
            }
            return axis_value < -threshold;
        }
        return false;
    }

private:
    const int port;
    const int axis;
    float threshold;
    bool trigger_if_greater;
    GCAdapter::Adapter* gcadapter;
    const float origin_value;
};

GCButtonFactory::GCButtonFactory(std::shared_ptr<GCAdapter::Adapter> adapter_)
    : adapter(std::move(adapter_)) {}

GCButton::~GCButton() = default;

std::unique_ptr<Input::ButtonDevice> GCButtonFactory::Create(const Common::ParamPackage& params) {
    const int button_id = params.Get("button", 0);
    const int port = params.Get("port", 0);

    constexpr int PAD_STICK_ID = static_cast<u16>(GCAdapter::PadButton::PAD_STICK);

    // button is not an axis/stick button
    if (button_id != PAD_STICK_ID) {
        return std::make_unique<GCButton>(port, button_id, adapter.get());
    }

    // For Axis buttons, used by the binary sticks.
    if (button_id == PAD_STICK_ID) {
        const int axis = params.Get("axis", 0);
        const float threshold = params.Get("threshold", 0.25f);
        const std::string direction_name = params.Get("direction", "");
        bool trigger_if_greater;
        if (direction_name == "+") {
            trigger_if_greater = true;
        } else if (direction_name == "-") {
            trigger_if_greater = false;
        } else {
            trigger_if_greater = true;
            LOG_ERROR(Input, "Unknown direction {}", direction_name);
        }
        return std::make_unique<GCAxisButton>(port, axis, threshold, trigger_if_greater,
                                              adapter.get());
    }

    UNREACHABLE();
    return nullptr;
}

Common::ParamPackage GCButtonFactory::GetNextInput() {
    Common::ParamPackage params;
    GCAdapter::GCPadStatus pad;
    auto& queue = adapter->GetPadQueue();
    for (std::size_t port = 0; port < queue.size(); ++port) {
        while (queue[port].Pop(pad)) {
            // This while loop will break on the earliest detected button
            params.Set("engine", "gcpad");
            params.Set("port", static_cast<int>(port));
            for (const auto& button : GCAdapter::PadButtonArray) {
                const u16 button_value = static_cast<u16>(button);
                if (pad.button & button_value) {
                    params.Set("button", button_value);
                    break;
                }
            }

            // For Axis button implementation
            if (pad.axis != GCAdapter::PadAxes::Undefined) {
                params.Set("axis", static_cast<u8>(pad.axis));
                params.Set("button", static_cast<u16>(GCAdapter::PadButton::PAD_STICK));
                if (pad.axis_value > 128) {
                    params.Set("direction", "+");
                    params.Set("threshold", "0.25");
                } else {
                    params.Set("direction", "-");
                    params.Set("threshold", "-0.25");
                }
                break;
            }
        }
    }
    return params;
}

void GCButtonFactory::Start() {
    polling = true;
    adapter->BeginConfiguration();
}

void GCButtonFactory::Stop() {
    polling = false;
    adapter->EndConfiguration();
}

class GCAnalog final : public Input::AnalogDevice {
public:
    GCAnalog(int port_, int axis_x_, int axis_y_, float deadzone_, GCAdapter::Adapter* adapter,
             float range_)
        : port(port_), axis_x(axis_x_), axis_y(axis_y_), deadzone(deadzone_), gcadapter(adapter),
          origin_value_x(static_cast<float>(adapter->GetOriginValue(port_, axis_x_))),
          origin_value_y(static_cast<float>(adapter->GetOriginValue(port_, axis_y_))),
          range(range_) {}

    float GetAxis(int axis) const {
        if (gcadapter->DeviceConnected(port)) {
            std::lock_guard lock{mutex};
            const auto origin_value = axis % 2 == 0 ? origin_value_x : origin_value_y;
            return (gcadapter->GetPadState()[port].axes.at(axis) - origin_value) / (100.0f * range);
        }
        return 0.0f;
    }

    std::pair<float, float> GetAnalog(int axis_x, int axis_y) const {
        float x = GetAxis(axis_x);
        float y = GetAxis(axis_y);

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return {x, y};
    }

    std::tuple<float, float> GetStatus() const override {
        const auto [x, y] = GetAnalog(axis_x, axis_y);
        const float r = std::sqrt((x * x) + (y * y));
        if (r > deadzone) {
            return {x / r * (r - deadzone) / (1 - deadzone),
                    y / r * (r - deadzone) / (1 - deadzone)};
        }
        return {0.0f, 0.0f};
    }

private:
    const int port;
    const int axis_x;
    const int axis_y;
    const float deadzone;
    GCAdapter::Adapter* gcadapter;
    const float origin_value_x;
    const float origin_value_y;
    const float range;
    mutable std::mutex mutex;
};

/// An analog device factory that creates analog devices from GC Adapter
GCAnalogFactory::GCAnalogFactory(std::shared_ptr<GCAdapter::Adapter> adapter_)
    : adapter(std::move(adapter_)) {}

/**
 * Creates analog device from joystick axes
 * @param params contains parameters for creating the device:
 *     - "port": the nth gcpad on the adapter
 *     - "axis_x": the index of the axis to be bind as x-axis
 *     - "axis_y": the index of the axis to be bind as y-axis
 */
std::unique_ptr<Input::AnalogDevice> GCAnalogFactory::Create(const Common::ParamPackage& params) {
    const int port = params.Get("port", 0);
    const int axis_x = params.Get("axis_x", 0);
    const int axis_y = params.Get("axis_y", 1);
    const float deadzone = std::clamp(params.Get("deadzone", 0.0f), 0.0f, .99f);
    const float range = std::clamp(params.Get("range", 1.0f), 0.50f, 1.50f);

    return std::make_unique<GCAnalog>(port, axis_x, axis_y, deadzone, adapter.get(), range);
}

void GCAnalogFactory::Start() {
    polling = true;
    adapter->BeginConfiguration();
}

void GCAnalogFactory::Stop() {
    polling = false;
    adapter->EndConfiguration();
}

Common::ParamPackage GCAnalogFactory::GetNextInput() {
    GCAdapter::GCPadStatus pad;
    auto& queue = adapter->GetPadQueue();
    for (std::size_t port = 0; port < queue.size(); ++port) {
        while (queue[port].Pop(pad)) {
            if (pad.axis == GCAdapter::PadAxes::Undefined ||
                std::abs((pad.axis_value - 128.0f) / 128.0f) < 0.05) {
                continue;
            }
            // An analog device needs two axes, so we need to store the axis for later and wait for
            // a second input event. The axes also must be from the same joystick.
            const u8 axis = static_cast<u8>(pad.axis);
            if (analog_x_axis == -1) {
                analog_x_axis = axis;
                controller_number = static_cast<int>(port);
            } else if (analog_y_axis == -1 && analog_x_axis != axis &&
                       controller_number == static_cast<int>(port)) {
                analog_y_axis = axis;
            }
        }
    }
    Common::ParamPackage params;
    if (analog_x_axis != -1 && analog_y_axis != -1) {
        params.Set("engine", "gcpad");
        params.Set("port", controller_number);
        params.Set("axis_x", analog_x_axis);
        params.Set("axis_y", analog_y_axis);
        analog_x_axis = -1;
        analog_y_axis = -1;
        controller_number = -1;
        return params;
    }
    return params;
}

} // namespace InputCommon
