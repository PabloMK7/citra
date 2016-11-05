// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "core/loader/loader.h"

class EmuWindow;

namespace Core {

class System {
public:
    struct State {
        std::unique_ptr<Loader::AppLoader> app_loader;
    };

    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Initialize and Load process.
    enum class ResultStatus : u32 {
        Success, ///< Succeeded
        ErrorGetLoader, ///< Error finding the correct application loader
        ErrorSystemMode, ///< Error determining the system mode
        ErrorLoader, ///< Error loading the specified application
        ErrorLoader_ErrorEncrypted, ///< Error loading the specified application due to encryption
        ErrorLoader_ErrorInvalidFormat, ///< Error loading the specified application due to an invalid format
        ErrorVideoCore, ///< Error in the video core
    };

    /**
     * Initialize the emulated system.
     * @param emu_window Pointer to the host-system window used for video output and keyboard input.
     * @param system_mode The system mode.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Init(EmuWindow* emu_window, u32 system_mode);

    /// Shutdown the emulated system.
    void Shutdown();

    /**
     * Load an executable application.
     * @param emu_window Pointer to the host-system window used for video output and keyboard input.
     * @param filepath String path to the executable application to load on the host file system.
     * @returns ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Load(EmuWindow* emu_window, const std::string& filepath);

    /**
     * Indicates if the emulated system is powered on (all subsystems initialized and able to run an
     * application).
     * @returns True if the emulated system is powered on, otherwise false.
     */
    bool IsPoweredOn() const {
        return is_powered_on;
    }

    /**
     * Gets the internal state of the emulated system.
     * @returns The internal state of the emulated system
     */
    State& GetState() {
        return state;
    }

private:
    bool is_powered_on{};
    State state;

    static System s_instance;
};

} // namespace Core
