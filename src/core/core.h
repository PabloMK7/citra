// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "common/common_types.h"
#include "core/memory.h"

class EmuWindow;
class ARM_Interface;

namespace Loader {
class AppLoader;
}

namespace Core {

struct ThreadContext {
    u32 cpu_registers[13];
    u32 sp;
    u32 lr;
    u32 pc;
    u32 cpsr;
    u32 fpu_registers[64];
    u32 fpscr;
    u32 fpexc;
};

class System {
public:
    /**
     * Gets the instance of the System singleton class.
     * @returns Reference to the instance of the System singleton class.
     */
    static System& GetInstance() {
        return s_instance;
    }

    /// Enumeration representing the return values of the System Initialize and Load process.
    enum class ResultStatus : u32 {
        Success,                    ///< Succeeded
        ErrorNotInitialized,        ///< Error trying to use core prior to initialization
        ErrorGetLoader,             ///< Error finding the correct application loader
        ErrorSystemMode,            ///< Error determining the system mode
        ErrorLoader,                ///< Error loading the specified application
        ErrorLoader_ErrorEncrypted, ///< Error loading the specified application due to encryption
        ErrorLoader_ErrorInvalidFormat, ///< Error loading the specified application due to an
                                        /// invalid format
        ErrorVideoCore,                 ///< Error in the video core
    };

    /**
     * Run the core CPU loop
     * This function runs the core for the specified number of CPU instructions before trying to
     * update hardware. This is much faster than SingleStep (and should be equivalent), as the CPU
     * is not required to do a full dispatch with each instruction. NOTE: the number of instructions
     * requested is not guaranteed to run, as this will be interrupted preemptively if a hardware
     * update is requested (e.g. on a thread switch).
     * @param tight_loop Number of instructions to execute.
     * @return Result status, indicating whethor or not the operation succeeded.
     */
    ResultStatus RunLoop(int tight_loop = 1000);

    /**
     * Step the CPU one instruction
     * @return Result status, indicating whethor or not the operation succeeded.
     */
    ResultStatus SingleStep();

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
        return app_core != nullptr;
    }

    /// Prepare the core emulation for a reschedule
    void PrepareReschedule();

    /**
     * Gets a reference to the emulated AppCore CPU.
     * @returns A reference to the emulated AppCore CPU.
     */
    ARM_Interface& AppCore() {
        return *app_core;
    }

private:
    /**
     * Initialize the emulated system.
     * @param emu_window Pointer to the host-system window used for video output and keyboard input.
     * @param system_mode The system mode.
     * @return ResultStatus code, indicating if the operation succeeded.
     */
    ResultStatus Init(EmuWindow* emu_window, u32 system_mode);

    /// Reschedule the core emulation
    void Reschedule();

    /// AppLoader used to load the current executing application
    std::unique_ptr<Loader::AppLoader> app_loader;

    ///< ARM11 application core
    std::unique_ptr<ARM_Interface> app_core;

    /// When true, signals that a reschedule should happen
    bool reschedule_pending{};

    static System s_instance;
};

static ARM_Interface& AppCore() {
    return System::GetInstance().AppCore();
}

} // namespace Core
