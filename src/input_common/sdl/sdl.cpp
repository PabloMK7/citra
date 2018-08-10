// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <SDL.h>
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "input_common/main.h"
#include "input_common/sdl/sdl.h"

namespace InputCommon {

namespace SDL {

class SDLJoystick;
class SDLButtonFactory;
class SDLAnalogFactory;
static std::unordered_map<int, std::weak_ptr<SDLJoystick>> joystick_list;
static std::shared_ptr<SDLButtonFactory> button_factory;
static std::shared_ptr<SDLAnalogFactory> analog_factory;

static bool initialized = false;

class SDLJoystick {
public:
    explicit SDLJoystick(int joystick_index)
        : joystick{SDL_JoystickOpen(joystick_index), SDL_JoystickClose} {
        if (!joystick) {
            LOG_ERROR(Input, "failed to open joystick {}", joystick_index);
        }
    }

    bool GetButton(int button) const {
        if (!joystick)
            return {};
        SDL_JoystickUpdate();
        return SDL_JoystickGetButton(joystick.get(), button) == 1;
    }

    float GetAxis(int axis) const {
        if (!joystick)
            return {};
        SDL_JoystickUpdate();
        return SDL_JoystickGetAxis(joystick.get(), axis) / 32767.0f;
    }

    std::tuple<float, float> GetAnalog(int axis_x, int axis_y) const {
        float x = GetAxis(axis_x);
        float y = GetAxis(axis_y);
        y = -y; // 3DS uses an y-axis inverse from SDL

        // Make sure the coordinates are in the unit circle,
        // otherwise normalize it.
        float r = x * x + y * y;
        if (r > 1.0f) {
            r = std::sqrt(r);
            x /= r;
            y /= r;
        }

        return std::make_tuple(x, y);
    }

    bool GetHatDirection(int hat, Uint8 direction) const {
        return (SDL_JoystickGetHat(joystick.get(), hat) & direction) != 0;
    }

    SDL_JoystickID GetJoystickID() const {
        return SDL_JoystickInstanceID(joystick.get());
    }

private:
    std::unique_ptr<SDL_Joystick, decltype(&SDL_JoystickClose)> joystick;
};

class SDLButton final : public Input::ButtonDevice {
public:
    explicit SDLButton(std::shared_ptr<SDLJoystick> joystick_, int button_)
        : joystick(std::move(joystick_)), button(button_) {}

    bool GetStatus() const override {
        return joystick->GetButton(button);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int button;
};

class SDLDirectionButton final : public Input::ButtonDevice {
public:
    explicit SDLDirectionButton(std::shared_ptr<SDLJoystick> joystick_, int hat_, Uint8 direction_)
        : joystick(std::move(joystick_)), hat(hat_), direction(direction_) {}

    bool GetStatus() const override {
        return joystick->GetHatDirection(hat, direction);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int hat;
    Uint8 direction;
};

class SDLAxisButton final : public Input::ButtonDevice {
public:
    explicit SDLAxisButton(std::shared_ptr<SDLJoystick> joystick_, int axis_, float threshold_,
                           bool trigger_if_greater_)
        : joystick(std::move(joystick_)), axis(axis_), threshold(threshold_),
          trigger_if_greater(trigger_if_greater_) {}

    bool GetStatus() const override {
        float axis_value = joystick->GetAxis(axis);
        if (trigger_if_greater)
            return axis_value > threshold;
        return axis_value < threshold;
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int axis;
    float threshold;
    bool trigger_if_greater;
};

class SDLAnalog final : public Input::AnalogDevice {
public:
    SDLAnalog(std::shared_ptr<SDLJoystick> joystick_, int axis_x_, int axis_y_)
        : joystick(std::move(joystick_)), axis_x(axis_x_), axis_y(axis_y_) {}

    std::tuple<float, float> GetStatus() const override {
        return joystick->GetAnalog(axis_x, axis_y);
    }

private:
    std::shared_ptr<SDLJoystick> joystick;
    int axis_x;
    int axis_y;
};

static std::shared_ptr<SDLJoystick> GetJoystick(int joystick_index) {
    std::shared_ptr<SDLJoystick> joystick = joystick_list[joystick_index].lock();
    if (!joystick) {
        joystick = std::make_shared<SDLJoystick>(joystick_index);
        joystick_list[joystick_index] = joystick;
    }
    return joystick;
}

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "joystick": the index of the joystick to bind
     *     - "button"(optional): the index of the button to bind
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis value
     *         is greater than the threshold; "-" means the button is triggered when the axis value
     *         is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const int joystick_index = params.Get("joystick", 0);

        if (params.Has("hat")) {
            const int hat = params.Get("hat", 0);
            const std::string direction_name = params.Get("direction", "");
            Uint8 direction;
            if (direction_name == "up") {
                direction = SDL_HAT_UP;
            } else if (direction_name == "down") {
                direction = SDL_HAT_DOWN;
            } else if (direction_name == "left") {
                direction = SDL_HAT_LEFT;
            } else if (direction_name == "right") {
                direction = SDL_HAT_RIGHT;
            } else {
                direction = 0;
            }
            return std::make_unique<SDLDirectionButton>(GetJoystick(joystick_index), hat,
                                                        direction);
        }

        if (params.Has("axis")) {
            const int axis = params.Get("axis", 0);
            const float threshold = params.Get("threshold", 0.5f);
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
            return std::make_unique<SDLAxisButton>(GetJoystick(joystick_index), axis, threshold,
                                                   trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        return std::make_unique<SDLButton>(GetJoystick(joystick_index), button);
    }
};

/// An analog device factory that creates analog devices from SDL joystick
class SDLAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    /**
     * Creates analog device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "joystick": the index of the joystick to bind
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const int joystick_index = params.Get("joystick", 0);
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);
        return std::make_unique<SDLAnalog>(GetJoystick(joystick_index), axis_x, axis_y);
    }
};

void Init() {
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(SDL_INIT_JOYSTICK) failed with: {}", SDL_GetError());
    } else {
        using namespace Input;
        RegisterFactory<ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>());
        RegisterFactory<AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>());
        initialized = true;
    }
}

void Shutdown() {
    if (initialized) {
        using namespace Input;
        UnregisterFactory<ButtonDevice>("sdl");
        UnregisterFactory<AnalogDevice>("sdl");
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
    }
}

/**
 * This function converts a joystick ID used in SDL events to the device index. This is necessary
 * because Citra opens joysticks using their indices, not their IDs.
 */
static int JoystickIDToDeviceIndex(SDL_JoystickID id) {
    int num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; i++) {
        auto joystick = GetJoystick(i);
        if (joystick->GetJoystickID() == id) {
            return i;
        }
    }
    return -1;
}

Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event) {
    Common::ParamPackage params({{"engine", "sdl"}});
    switch (event.type) {
    case SDL_JOYAXISMOTION:
        params.Set("joystick", JoystickIDToDeviceIndex(event.jaxis.which));
        params.Set("axis", event.jaxis.axis);
        if (event.jaxis.value > 0) {
            params.Set("direction", "+");
            params.Set("threshold", "0.5");
        } else {
            params.Set("direction", "-");
            params.Set("threshold", "-0.5");
        }
        break;
    case SDL_JOYBUTTONUP:
        params.Set("joystick", JoystickIDToDeviceIndex(event.jbutton.which));
        params.Set("button", event.jbutton.button);
        break;
    case SDL_JOYHATMOTION:
        params.Set("joystick", JoystickIDToDeviceIndex(event.jhat.which));
        params.Set("hat", event.jhat.hat);
        switch (event.jhat.value) {
        case SDL_HAT_UP:
            params.Set("direction", "up");
            break;
        case SDL_HAT_DOWN:
            params.Set("direction", "down");
            break;
        case SDL_HAT_LEFT:
            params.Set("direction", "left");
            break;
        case SDL_HAT_RIGHT:
            params.Set("direction", "right");
            break;
        default:
            return {};
        }
        break;
    }
    return params;
}

namespace Polling {

class SDLPoller : public InputCommon::Polling::DevicePoller {
public:
    void Start() override {
        // SDL joysticks must be opened, otherwise they don't generate events
        SDL_JoystickUpdate();
        int num_joysticks = SDL_NumJoysticks();
        for (int i = 0; i < num_joysticks; i++) {
            joysticks_opened.emplace_back(GetJoystick(i));
        }
        // Empty event queue to get rid of old events. citra-qt doesn't use the queue
        SDL_Event dummy;
        while (SDL_PollEvent(&dummy)) {
        }
    }

    void Stop() override {
        joysticks_opened.clear();
    }

private:
    std::vector<std::shared_ptr<SDLJoystick>> joysticks_opened;
};

class SDLButtonPoller final : public SDLPoller {
public:
    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_JOYAXISMOTION:
                if (std::abs(event.jaxis.value / 32767.0) < 0.5) {
                    break;
                }
            case SDL_JOYBUTTONUP:
            case SDL_JOYHATMOTION:
                return SDLEventToButtonParamPackage(event);
            }
        }
        return {};
    }
};

class SDLAnalogPoller final : public SDLPoller {
public:
    void Start() override {
        SDLPoller::Start();

        // Reset stored axes
        analog_xaxis = -1;
        analog_yaxis = -1;
        analog_axes_joystick = -1;
    }

    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type != SDL_JOYAXISMOTION || std::abs(event.jaxis.value / 32767.0) < 0.5) {
                continue;
            }
            // An analog device needs two axes, so we need to store the axis for later and wait for
            // a second SDL event. The axes also must be from the same joystick.
            int axis = event.jaxis.axis;
            if (analog_xaxis == -1) {
                analog_xaxis = axis;
                analog_axes_joystick = event.jaxis.which;
            } else if (analog_yaxis == -1 && analog_xaxis != axis &&
                       analog_axes_joystick == event.jaxis.which) {
                analog_yaxis = axis;
            }
        }
        Common::ParamPackage params;
        if (analog_xaxis != -1 && analog_yaxis != -1) {
            params.Set("engine", "sdl");
            params.Set("joystick", JoystickIDToDeviceIndex(analog_axes_joystick));
            params.Set("axis_x", analog_xaxis);
            params.Set("axis_y", analog_yaxis);
            analog_xaxis = -1;
            analog_yaxis = -1;
            analog_axes_joystick = -1;
            return params;
        }
        return params;
    }

private:
    int analog_xaxis = -1;
    int analog_yaxis = -1;
    SDL_JoystickID analog_axes_joystick = -1;
};

void GetPollers(InputCommon::Polling::DeviceType type,
                std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>>& pollers) {
    switch (type) {
    case InputCommon::Polling::DeviceType::Analog:
        pollers.emplace_back(std::make_unique<SDLAnalogPoller>());
        break;
    case InputCommon::Polling::DeviceType::Button:
        pollers.emplace_back(std::make_unique<SDLButtonPoller>());
        break;
    }
}
} // namespace Polling
} // namespace SDL
} // namespace InputCommon
