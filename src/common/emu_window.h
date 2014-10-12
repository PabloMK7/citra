// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "common/key_map.h"

// Abstraction class used to provide an interface between emulation code and the frontend (e.g. SDL, 
//  QGLWidget, GLFW, etc...)
class EmuWindow
{

public:
    /// Data structure to store an emuwindow configuration
    struct WindowConfig {
        bool    fullscreen;
        int     res_width;
        int     res_height;
        std::pair<unsigned,unsigned> min_client_area_size;
    };

    /// Swap buffers to display the next frame
    virtual void SwapBuffers() = 0;

    /// Polls window events
    virtual void PollEvents() = 0;

    /// Makes the graphics context current for the caller thread
    virtual void MakeCurrent() = 0;

    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    virtual void DoneCurrent() = 0;

    virtual void ReloadSetKeymaps() = 0;

    /// Signals a key press action to the HID module
    static void KeyPressed(KeyMap::HostDeviceKey key);

    /// Signals a key release action to the HID module
    static void KeyReleased(KeyMap::HostDeviceKey key);

    const WindowConfig& GetActiveConfig() const {
        return active_config;
    }

    void SetConfig(const WindowConfig& val) {
        config = val;
    }

    /**
      * Gets the size of the framebuffer in pixels
      */
    const std::pair<unsigned,unsigned> GetFramebufferSize() const {
        return framebuffer_size;
    }

    /**
     * Gets window client area width in logical coordinates
     */
    std::pair<unsigned,unsigned> GetClientAreaSize() const {
        return std::make_pair(client_area_width, client_area_height);
    }

    std::string GetWindowTitle() const {
        return window_title;
    }

    void SetWindowTitle(const std::string& val) {
        window_title = val;
    }

    // Only call this from the GUI thread!
    void ProcessConfigurationChanges() {
        // TODO: For proper thread safety, we should eventually implement a proper
        // multiple-writer/single-reader queue...

        if (config.min_client_area_size != active_config.min_client_area_size) {
            OnMinimalClientAreaChangeRequest(config.min_client_area_size);
            config.min_client_area_size = active_config.min_client_area_size;
        }
    }

protected:
    EmuWindow() :
        window_title(Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc))
    {
        // TODO
        config.min_client_area_size = std::make_pair(300u, 500u);
        active_config = config;
    }
    virtual ~EmuWindow() {}

    std::pair<unsigned,unsigned> NotifyFramebufferSizeChanged(const std::pair<unsigned,unsigned>& size) {
        framebuffer_size = size;
    }

    void NotifyClientAreaSizeChanged(const std::pair<unsigned,unsigned>& size) {
        client_area_width = size.first;
        client_area_height = size.second;
    }

private:
    virtual void OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) {
    }

    std::string window_title;      ///< Current window title, should be used by window impl.

    std::pair<unsigned,unsigned> framebuffer_size;

    unsigned client_area_width;    ///< Current client width, should be set by window impl.
    unsigned client_area_height;   ///< Current client height, should be set by window impl.

    WindowConfig config;         ///< Internal configuration (changes pending for being applied in ProcessConfigurationChanges)
    WindowConfig active_config;  ///< Internal active configuration
};
