// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <SDL.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/param_package.h"
#include "common/threadsafe_queue.h"
#include "input_common/main.h"
#include "input_common/sdl/sdl.h"

namespace InputCommon {

namespace SDL {

class SDLJoystick;
class SDLButtonFactory;
class SDLAnalogFactory;

/// Map of GUID of a list of corresponding virtual Joysticks
static std::unordered_map<std::string, std::vector<std::shared_ptr<SDLJoystick>>> joystick_map;
static std::mutex joystick_map_mutex;

static std::shared_ptr<SDLButtonFactory> button_factory;
static std::shared_ptr<SDLAnalogFactory> analog_factory;

/// Used by the Pollers during config
static std::atomic<bool> polling;
static Common::SPSCQueue<SDL_Event> event_queue;

static std::atomic<bool> initialized = false;

static std::string GetGUID(SDL_Joystick* joystick) {
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
    char guid_str[33];
    SDL_JoystickGetGUIDString(guid, guid_str, sizeof(guid_str));
    return guid_str;
}

class SDLJoystick {
public:
    SDLJoystick(std::string guid_, int port_, SDL_Joystick* joystick,
                decltype(&SDL_JoystickClose) deleter = &SDL_JoystickClose)
        : guid{std::move(guid_)}, port{port_}, sdl_joystick{joystick, deleter} {}

    void SetButton(int button, bool value) {
        std::lock_guard<std::mutex> lock(mutex);
        state.buttons[button] = value;
    }

    bool GetButton(int button) const {
        std::lock_guard<std::mutex> lock(mutex);
        return state.buttons.at(button);
    }

    void SetAxis(int axis, Sint16 value) {
        std::lock_guard<std::mutex> lock(mutex);
        state.axes[axis] = value;
    }

    float GetAxis(int axis) const {
        std::lock_guard<std::mutex> lock(mutex);
        return state.axes.at(axis) / 32767.0f;
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

    void SetHat(int hat, Uint8 direction) {
        std::lock_guard<std::mutex> lock(mutex);
        state.hats[hat] = direction;
    }

    bool GetHatDirection(int hat, Uint8 direction) const {
        std::lock_guard<std::mutex> lock(mutex);
        return (state.hats.at(hat) & direction) != 0;
    }
    /**
     * The guid of the joystick
     */
    const std::string& GetGUID() const {
        return guid;
    }

    /**
     * The number of joystick from the same type that were connected before this joystick
     */
    int GetPort() const {
        return port;
    }

    SDL_Joystick* GetSDLJoystick() const {
        return sdl_joystick.get();
    }

    void SetSDLJoystick(SDL_Joystick* joystick,
                        decltype(&SDL_JoystickClose) deleter = &SDL_JoystickClose) {
        sdl_joystick =
            std::unique_ptr<SDL_Joystick, decltype(&SDL_JoystickClose)>(joystick, deleter);
    }

private:
    struct State {
        std::unordered_map<int, bool> buttons;
        std::unordered_map<int, Sint16> axes;
        std::unordered_map<int, Uint8> hats;
    } state;
    std::string guid;
    int port;
    std::unique_ptr<SDL_Joystick, decltype(&SDL_JoystickClose)> sdl_joystick;
    mutable std::mutex mutex;
};

/**
 * Get the nth joystick with the corresponding GUID
 */
static std::shared_ptr<SDLJoystick> GetSDLJoystickByGUID(const std::string& guid, int port) {
    std::lock_guard<std::mutex> lock(joystick_map_mutex);
    const auto it = joystick_map.find(guid);
    if (it != joystick_map.end()) {
        while (it->second.size() <= port) {
            auto joystick = std::make_shared<SDLJoystick>(guid, it->second.size(), nullptr,
                                                          [](SDL_Joystick*) {});
            it->second.emplace_back(std::move(joystick));
        }
        return it->second[port];
    }
    auto joystick = std::make_shared<SDLJoystick>(guid, 0, nullptr, [](SDL_Joystick*) {});
    return joystick_map[guid].emplace_back(std::move(joystick));
}

/**
 * Check how many identical joysticks (by guid) were connected before the one with sdl_id and so tie
 * it to a SDLJoystick with the same guid and that port
 */
static std::shared_ptr<SDLJoystick> GetSDLJoystickBySDLID(SDL_JoystickID sdl_id) {
    std::lock_guard<std::mutex> lock(joystick_map_mutex);
    auto sdl_joystick = SDL_JoystickFromInstanceID(sdl_id);
    const std::string guid = GetGUID(sdl_joystick);
    auto map_it = joystick_map.find(guid);
    if (map_it != joystick_map.end()) {
        auto vec_it = std::find_if(map_it->second.begin(), map_it->second.end(),
                                   [&sdl_joystick](const std::shared_ptr<SDLJoystick>& joystick) {
                                       return sdl_joystick == joystick->GetSDLJoystick();
                                   });
        if (vec_it != map_it->second.end()) {
            // This is the common case: There is already an existing SDL_Joystick maped to a
            // SDLJoystick. return the SDLJoystick
            return *vec_it;
        }
        // Search for a SDLJoystick without a mapped SDL_Joystick...
        auto nullptr_it = std::find_if(map_it->second.begin(), map_it->second.end(),
                                       [](const std::shared_ptr<SDLJoystick>& joystick) {
                                           return !joystick->GetSDLJoystick();
                                       });
        if (nullptr_it != map_it->second.end()) {
            // ... and map it
            (*nullptr_it)->SetSDLJoystick(sdl_joystick);
            return *nullptr_it;
        }
        // There is no SDLJoystick without a mapped SDL_Joystick
        // Create a new SDLJoystick
        auto joystick = std::make_shared<SDLJoystick>(guid, map_it->second.size(), sdl_joystick);
        return map_it->second.emplace_back(std::move(joystick));
    }
    auto joystick = std::make_shared<SDLJoystick>(guid, 0, sdl_joystick);
    return joystick_map[guid].emplace_back(std::move(joystick));
}

void InitJoystick(int joystick_index) {
    std::lock_guard<std::mutex> lock(joystick_map_mutex);
    SDL_Joystick* sdl_joystick = SDL_JoystickOpen(joystick_index);
    if (!sdl_joystick) {
        LOG_ERROR(Input, "failed to open joystick {}", joystick_index);
        return;
    }
    std::string guid = GetGUID(sdl_joystick);
    if (joystick_map.find(guid) == joystick_map.end()) {
        auto joystick = std::make_shared<SDLJoystick>(guid, 0, sdl_joystick);
        joystick_map[guid].emplace_back(std::move(joystick));
        return;
    }
    auto& joystick_guid_list = joystick_map[guid];
    const auto it = std::find_if(
        joystick_guid_list.begin(), joystick_guid_list.end(),
        [](const std::shared_ptr<SDLJoystick>& joystick) { return !joystick->GetSDLJoystick(); });
    if (it != joystick_guid_list.end()) {
        (*it)->SetSDLJoystick(sdl_joystick);
        return;
    }
    auto joystick = std::make_shared<SDLJoystick>(guid, joystick_guid_list.size(), sdl_joystick);
    joystick_guid_list.emplace_back(std::move(joystick));
}

void CloseJoystick(SDL_Joystick* sdl_joystick) {
    std::lock_guard<std::mutex> lock(joystick_map_mutex);
    std::string guid = GetGUID(sdl_joystick);
    // This call to guid is save since the joystick is guranteed to be in that map
    auto& joystick_guid_list = joystick_map[guid];
    const auto joystick_it =
        std::find_if(joystick_guid_list.begin(), joystick_guid_list.end(),
                     [&sdl_joystick](const std::shared_ptr<SDLJoystick>& joystick) {
                         return joystick->GetSDLJoystick() == sdl_joystick;
                     });
    (*joystick_it)->SetSDLJoystick(nullptr, [](SDL_Joystick*) {});
}

void HandleGameControllerEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_JOYBUTTONUP: {
        auto joystick = GetSDLJoystickBySDLID(event.jbutton.which);
        if (joystick) {
            joystick->SetButton(event.jbutton.button, false);
        }
        break;
    }
    case SDL_JOYBUTTONDOWN: {
        auto joystick = GetSDLJoystickBySDLID(event.jbutton.which);
        if (joystick) {
            joystick->SetButton(event.jbutton.button, true);
        }
        break;
    }
    case SDL_JOYHATMOTION: {
        auto joystick = GetSDLJoystickBySDLID(event.jhat.which);
        if (joystick) {
            joystick->SetHat(event.jhat.hat, event.jhat.value);
        }
        break;
    }
    case SDL_JOYAXISMOTION: {
        auto joystick = GetSDLJoystickBySDLID(event.jaxis.which);
        if (joystick) {
            joystick->SetAxis(event.jaxis.axis, event.jaxis.value);
        }
        break;
    }
    case SDL_JOYDEVICEREMOVED:
        LOG_DEBUG(Input, "Controller removed with Instance_ID {}", event.jdevice.which);
        CloseJoystick(SDL_JoystickFromInstanceID(event.jdevice.which));
        break;
    case SDL_JOYDEVICEADDED:
        LOG_DEBUG(Input, "Controller connected with device index {}", event.jdevice.which);
        InitJoystick(event.jdevice.which);
        break;
    }
}

void CloseSDLJoysticks() {
    std::lock_guard<std::mutex> lock(joystick_map_mutex);
    joystick_map.clear();
}

void PollLoop() {
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        LOG_CRITICAL(Input, "SDL_Init(SDL_INIT_JOYSTICK) failed with: {}", SDL_GetError());
        return;
    }

    SDL_Event event;
    while (initialized) {
        // Wait for 10 ms or until an event happens
        if (SDL_WaitEventTimeout(&event, 10)) {
            // Don't handle the event if we are configuring
            if (polling) {
                event_queue.Push(event);
            } else {
                HandleGameControllerEvent(event);
            }
        }
    }
    CloseSDLJoysticks();
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}

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

/// A button device factory that creates button devices from SDL joystick
class SDLButtonFactory final : public Input::Factory<Input::ButtonDevice> {
public:
    /**
     * Creates a button device from a joystick button
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type to bind
     *     - "button"(optional): the index of the button to bind
     *     - "hat"(optional): the index of the hat to bind as direction buttons
     *     - "axis"(optional): the index of the axis to bind
     *     - "direction"(only used for hat): the direction name of the hat to bind. Can be "up",
     *         "down", "left" or "right"
     *     - "threshold"(only used for axis): a float value in (-1.0, 1.0) which the button is
     *         triggered if the axis value crosses
     *     - "direction"(only used for axis): "+" means the button is triggered when the axis
     * value is greater than the threshold; "-" means the button is triggered when the axis
     * value is smaller than the threshold
     */
    std::unique_ptr<Input::ButtonDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);

        auto joystick = GetSDLJoystickByGUID(guid, port);

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
            // This is necessary so accessing GetHat with hat won't crash
            joystick->SetHat(hat, SDL_HAT_CENTERED);
            return std::make_unique<SDLDirectionButton>(joystick, hat, direction);
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
            // This is necessary so accessing GetAxis with axis won't crash
            joystick->SetAxis(axis, 0);
            return std::make_unique<SDLAxisButton>(joystick, axis, threshold, trigger_if_greater);
        }

        const int button = params.Get("button", 0);
        // This is necessary so accessing GetButton with button won't crash
        joystick->SetButton(button, false);
        return std::make_unique<SDLButton>(joystick, button);
    }
};

/// An analog device factory that creates analog devices from SDL joystick
class SDLAnalogFactory final : public Input::Factory<Input::AnalogDevice> {
public:
    /**
     * Creates analog device from joystick axes
     * @param params contains parameters for creating the device:
     *     - "guid": the guid of the joystick to bind
     *     - "port": the nth joystick of the same type
     *     - "axis_x": the index of the axis to be bind as x-axis
     *     - "axis_y": the index of the axis to be bind as y-axis
     */
    std::unique_ptr<Input::AnalogDevice> Create(const Common::ParamPackage& params) override {
        const std::string guid = params.Get("guid", "0");
        const int port = params.Get("port", 0);
        const int axis_x = params.Get("axis_x", 0);
        const int axis_y = params.Get("axis_y", 1);

        auto joystick = GetSDLJoystickByGUID(guid, port);

        // This is necessary so accessing GetAxis with axis_x and axis_y won't crash
        joystick->SetAxis(axis_x, 0);
        joystick->SetAxis(axis_y, 0);
        return std::make_unique<SDLAnalog>(joystick, axis_x, axis_y);
    }
};

void Init() {
    using namespace Input;
    RegisterFactory<ButtonDevice>("sdl", std::make_shared<SDLButtonFactory>());
    RegisterFactory<AnalogDevice>("sdl", std::make_shared<SDLAnalogFactory>());
    polling = false;
    initialized = true;
}

void Shutdown() {
    if (initialized) {
        using namespace Input;
        UnregisterFactory<ButtonDevice>("sdl");
        UnregisterFactory<AnalogDevice>("sdl");
        initialized = false;
    }
}

Common::ParamPackage SDLEventToButtonParamPackage(const SDL_Event& event) {
    Common::ParamPackage params({{"engine", "sdl"}});
    switch (event.type) {
    case SDL_JOYAXISMOTION: {
        auto joystick = GetSDLJoystickBySDLID(event.jaxis.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
        params.Set("axis", event.jaxis.axis);
        if (event.jaxis.value > 0) {
            params.Set("direction", "+");
            params.Set("threshold", "0.5");
        } else {
            params.Set("direction", "-");
            params.Set("threshold", "-0.5");
        }
        break;
    }
    case SDL_JOYBUTTONUP: {
        auto joystick = GetSDLJoystickBySDLID(event.jbutton.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
        params.Set("button", event.jbutton.button);
        break;
    }
    case SDL_JOYHATMOTION: {
        auto joystick = GetSDLJoystickBySDLID(event.jhat.which);
        params.Set("port", joystick->GetPort());
        params.Set("guid", joystick->GetGUID());
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
    }
    return params;
}

namespace Polling {

class SDLPoller : public InputCommon::Polling::DevicePoller {
public:
    void Start() override {
        event_queue.Clear();
        polling = true;
    }

    void Stop() override {
        polling = false;
    }
};

class SDLButtonPoller final : public SDLPoller {
public:
    Common::ParamPackage GetNextInput() override {
        SDL_Event event;
        while (event_queue.Pop(event)) {
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
        while (event_queue.Pop(event)) {
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
            auto joystick = GetSDLJoystickBySDLID(event.jaxis.which);
            params.Set("engine", "sdl");
            params.Set("port", joystick->GetPort());
            params.Set("guid", joystick->GetGUID());
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
