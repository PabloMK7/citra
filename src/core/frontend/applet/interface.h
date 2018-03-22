// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>

namespace Frontend {

enum class AppletType {
    SoftwareKeyboard,
};

class AppletConfig {};
class AppletData {};

class AppletInterface {
public:
    virtual ~AppletInterface() = default;

    /**
     * On applet start, the applet specific configuration will be passed in along with the
     * framebuffer.
     */
    // virtual void Setup(const Config* /*,  framebuffer */) = 0;

    /**
     * Called on a fixed schedule to have the applet update any state such as the framebuffer.
     */
    virtual void Update() = 0;

    /**
     * Checked every update to see if the applet is still running. When the applet is done, the core
     * will call ReceiveData
     */
    virtual bool IsRunning() {
        return running;
    }

private:
    // framebuffer;
    std::atomic_bool running = false;
};

/**
 * Frontends call this method to pass a frontend applet implementation to the core. If the core
 * already has a applet registered, then this replaces the old applet
 *
 * @param applet - Frontend Applet implementation that the HLE applet code will launch
 * @param type - Which type of applet
 */
void RegisterFrontendApplet(std::shared_ptr<AppletInterface> applet, AppletType type);

/**
 * Frontends call this to prevent future requests
 */
void UnregisterFrontendApplet(AppletType type);

/**
 * Returns the Frontend Applet for the provided type
 */
std::shared_ptr<AppletInterface> GetRegisteredApplet(AppletType type);

} // namespace Frontend
