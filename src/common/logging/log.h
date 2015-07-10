// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Log {

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
    Trace,    ///< Extremely detailed and repetitive debugging information that is likely to
              ///  pollute logs.
    Debug,    ///< Less detailed debugging information.
    Info,     ///< Status information from important points during execution.
    Warning,  ///< Minor or potential problems found during execution of a task.
    Error,    ///< Major problems found during execution of a task that prevent it from being
              ///  completed.
    Critical, ///< Major problems during execution that threathen the stability of the entire
              ///  application.

    Count ///< Total number of logging levels
};

typedef u8 ClassType;

/**
 * Specifies the sub-system that generated the log message.
 *
 * @note If you add a new entry here, also add a corresponding one to `ALL_LOG_CLASSES` in backend.cpp.
 */
enum class Class : ClassType {
    Log,                        ///< Messages about the log system itself
    Common,                     ///< Library routines
    Common_Filesystem,          ///< Filesystem interface library
    Common_Memory,              ///< Memory mapping and management functions
    Core,                       ///< LLE emulation core
    Core_ARM11,                 ///< ARM11 CPU core
    Core_Timing,                ///< CoreTiming functions
    Config,                     ///< Emulator configuration (including commandline)
    Debug,                      ///< Debugging tools
    Debug_Emulated,             ///< Debug messages from the emulated programs
    Debug_GPU,                  ///< GPU debugging tools
    Debug_Breakpoint,           ///< Logging breakpoints and watchpoints
    Kernel,                     ///< The HLE implementation of the CTR kernel
    Kernel_SVC,                 ///< Kernel system calls
    Service,                    ///< HLE implementation of system services. Each major service
                                ///  should have its own subclass.
    Service_SRV,                ///< The SRV (Service Directory) implementation
    Service_FS,                 ///< The FS (Filesystem) service implementation
    Service_ERR,                ///< The ERR (Error) port implementation
    Service_APT,                ///< The APT (Applets) service
    Service_GSP,                ///< The GSP (GPU control) service
    Service_AC,                 ///< The AC (WiFi status) service
    Service_AM,                 ///< The AM (Application manager) service
    Service_PTM,                ///< The PTM (Power status & misc.) service
    Service_LDR,                ///< The LDR (3ds dll loader) service
    Service_NIM,                ///< The NIM (Network interface manager) service
    Service_NWM,                ///< The NWM (Network wlan manager) service
    Service_CFG,                ///< The CFG (Configuration) service
    Service_DSP,                ///< The DSP (DSP control) service
    Service_HID,                ///< The HID (Human interface device) service
    Service_SOC,                ///< The SOC (Socket) service
    Service_Y2R,                ///< The Y2R (YUV to RGB conversion) service
    HW,                         ///< Low-level hardware emulation
    HW_Memory,                  ///< Memory-map and address translation
    HW_LCD,                     ///< LCD register emulation
    HW_GPU,                     ///< GPU control emulation
    Frontend,                   ///< Emulator UI
    Render,                     ///< Emulator video output and hardware acceleration
    Render_Software,            ///< Software renderer backend
    Render_OpenGL,              ///< OpenGL backend
    Loader,                     ///< ROM loader

    Count ///< Total number of logging classes
};

/// Logs a message to the global logger.
void LogMessage(Class log_class, Level log_level,
    const char* filename, unsigned int line_nr, const char* function,
#ifdef _MSC_VER
    _Printf_format_string_
#endif
    const char* format, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 6, 7)))
#endif
    ;

} // namespace Log

#define LOG_GENERIC(log_class, log_level, ...) \
    ::Log::LogMessage(::Log::Class::log_class, ::Log::Level::log_level, \
        __FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef _DEBUG
#define LOG_TRACE(   log_class, ...) LOG_GENERIC(log_class, Trace,    __VA_ARGS__)
#else
#define LOG_TRACE(   log_class, ...) (void(0))
#endif

#define LOG_DEBUG(   log_class, ...) LOG_GENERIC(log_class, Debug,    __VA_ARGS__)
#define LOG_INFO(    log_class, ...) LOG_GENERIC(log_class, Info,     __VA_ARGS__)
#define LOG_WARNING( log_class, ...) LOG_GENERIC(log_class, Warning,  __VA_ARGS__)
#define LOG_ERROR(   log_class, ...) LOG_GENERIC(log_class, Error,    __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...) LOG_GENERIC(log_class, Critical, __VA_ARGS__)
