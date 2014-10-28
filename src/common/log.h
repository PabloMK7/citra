// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/msg_handler.h"
#include "common/logging/log.h"

#ifndef LOGGING
#define LOGGING
#endif

enum {
    OS_LEVEL,       // Printed by the emulated operating system
    NOTICE_LEVEL,   // VERY important information that is NOT errors. Like startup and OSReports.
    ERROR_LEVEL,    // Critical errors
    WARNING_LEVEL,  // Something is suspicious.
    INFO_LEVEL,     // General information.
    DEBUG_LEVEL,    // Detailed debugging - might make things slow.
};

namespace LogTypes
{

enum LOG_TYPE {
    //ACTIONREPLAY,
    //AUDIO,
    //AUDIO_INTERFACE,
    BOOT,
    //COMMANDPROCESSOR,
    COMMON,
    //CONSOLE,
    CONFIG,
    //DISCIO,
    //FILEMON,
    //DSPHLE,
    //DSPLLE,
    //DSP_MAIL,
    //DSPINTERFACE,
    //DVDINTERFACE,
    //DYNA_REC,
    //EXPANSIONINTERFACE,
    //GDB_STUB,
    ARM11,
    GSP,
    OSHLE,
    MASTER_LOG,
    MEMMAP,
    //MEMCARD_MANAGER,
    //OSREPORT,
    //PAD,
    //PROCESSORINTERFACE,
    //PIXELENGINE,
    //SERIALINTERFACE,
    //SP1,
    //STREAMINGINTERFACE,
    VIDEO,
    //VIDEOINTERFACE,
    LOADER,
    FILESYS,
    //WII_IPC_DVD,
    //WII_IPC_ES,
    //WII_IPC_FILEIO,
    //WII_IPC_HID,
    KERNEL,
    SVC,
    HLE,
    RENDER,
    GPU,
    HW,
    TIME,
    //NETPLAY,
    GUI,

    NUMBER_OF_LOGS // Must be last
};

// FIXME: should this be removed?
enum LOG_LEVELS {
    LOS = OS_LEVEL,
    LNOTICE = NOTICE_LEVEL,
    LERROR = ERROR_LEVEL,
    LWARNING = WARNING_LEVEL,
    LINFO = INFO_LEVEL,
    LDEBUG = DEBUG_LEVEL,
};

#define LOGTYPES_LEVELS LogTypes::LOG_LEVELS
#define LOGTYPES_TYPE LogTypes::LOG_TYPE

}  // namespace

void GenericLog(LOGTYPES_LEVELS level, LOGTYPES_TYPE type, const char*file, int line,
    const char* function, const char* fmt, ...)
#ifdef __GNUC__
        __attribute__((format(printf, 6, 7)))
#endif
        ;

#if defined LOGGING || defined _DEBUG || defined DEBUGFAST
#define MAX_LOGLEVEL LDEBUG
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL LWARNING
#endif // loglevel
#endif // logging

#ifdef _WIN32
#ifndef __func__
#define __func__ __FUNCTION__
#endif
#endif

// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...) { \
    if (v <= LogTypes::MAX_LOGLEVEL) \
        GenericLog(v, t, __FILE__, __LINE__, __func__, __VA_ARGS__); \
    }

//#define OS_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LOS, __VA_ARGS__) } while (0)
//#define ERROR_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__) } while (0)
//#define WARN_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__) } while (0)
//#define NOTICE_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LNOTICE, __VA_ARGS__) } while (0)
//#define INFO_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__) } while (0)
//#define DEBUG_LOG(t,...) do { GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__) } while (0)

#define OS_LOG(t,...) LOG_INFO(Common, __VA_ARGS__)
#define ERROR_LOG(t,...) LOG_ERROR(Common_Filesystem, __VA_ARGS__)
#define WARN_LOG(t,...) LOG_WARNING(Kernel_SVC, __VA_ARGS__)
#define NOTICE_LOG(t,...) LOG_INFO(Service, __VA_ARGS__)
#define INFO_LOG(t,...) LOG_INFO(Service_FS, __VA_ARGS__)
#define DEBUG_LOG(t,...) LOG_DEBUG(Common, __VA_ARGS__)

#if MAX_LOGLEVEL >= DEBUG_LEVEL
#define _dbg_assert_(_t_, _a_) \
    if (!(_a_)) {\
        ERROR_LOG(_t_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
                       __LINE__, __FILE__, __TIME__); \
        if (!PanicYesNo("*** Assertion (see log)***\n")) {Crash();} \
    }
#define _dbg_assert_msg_(_t_, _a_, ...)\
    if (!(_a_)) {\
        ERROR_LOG(_t_, __VA_ARGS__); \
        if (!PanicYesNo(__VA_ARGS__)) {Crash();} \
    }
#define _dbg_update_() Host_UpdateLogDisplay();

#else // not debug
#define _dbg_update_() ;

#ifndef _dbg_assert_
#define _dbg_assert_(_t_, _a_) {}
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) {}
#endif // dbg_assert
#endif // MAX_LOGLEVEL DEBUG

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)

#ifndef GEKKO
#ifdef _WIN32
#define _assert_msg_(_t_, _a_, _fmt_, ...)        \
    if (!(_a_)) {\
        if (!PanicYesNo(_fmt_, __VA_ARGS__)) {Crash();} \
    }
#else // not win32
#define _assert_msg_(_t_, _a_, _fmt_, ...)        \
    if (!(_a_)) {\
        if (!PanicYesNo(_fmt_, ##__VA_ARGS__)) {Crash();} \
    }
#endif // WIN32
#else // GEKKO
#define _assert_msg_(_t_, _a_, _fmt_, ...)
#endif
