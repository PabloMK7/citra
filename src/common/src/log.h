/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    log.h
 * @author  ShizZy <shizzy@6bit.net>
 * @date    2012-02-11
 * @brief   Common logging routines used throughout the project
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#ifndef COMMON_LOG_H_
#define COMMON_LOG_H_

#include "SDL.h" // Used for threading/mutexes

#include "common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Logging Macros

#if defined(_DEBUG) || defined(DEBUG) || defined(LOGGING)
/// Debug mode, show all logs
#define MAX_LOG_LEVEL logger::LDEBUG
#else
/// Non debug mode, only show critical logs
#define MAX_LOG_LEVEL logger::LWARNING
#endif

/// Logs a message ** Don't use directly **
#define _LOG_GENERIC(level, type, ...) \
    if (level <= MAX_LOG_LEVEL) { \
        LogGeneric(level, type, __FILE__, __LINE__, false, __VA_ARGS__); \
    }

/// Used for appending to the last logged message
#define LOG_APPEND(level, type, ...) \
    if (logger::level <= MAX_LOG_LEVEL) { \
        logger::LogGeneric(logger::level, logger::type, __FILE__, __LINE__, true, __VA_ARGS__); \
    }

/// Use this for printing an IMPORTANT notice to the logger
#define LOG_NOTICE(type, ...) _LOG_GENERIC(logger::LNOTICE, logger::type, __VA_ARGS__)

/// Use this for printing an error message to the logger
#define LOG_ERROR(type, ...) _LOG_GENERIC(logger::LERROR, logger::type, __VA_ARGS__)

/// Use this for printing a crash report to the logger
#define LOG_CRASH(type, ...) _LOG_GENERIC(logger::LCRASH, logger::type, __VA_ARGS__)

/// Use this for printing a warning to the logger
#define LOG_WARNING(type, ...) _LOG_GENERIC(logger::LWARNING, logger::type, __VA_ARGS__)

/// Use this for printing general information to the logger
#define LOG_INFO(type, ...) _LOG_GENERIC(logger::LINFO, logger::type, __VA_ARGS__)

#if defined(_DEBUG) || defined(DEBUG) || defined(LOGGING)

/// Use this for printing a debug message to the logger
#define LOG_DEBUG(type, ...) _LOG_GENERIC(logger::LDEBUG, logger::type, __VA_ARGS__)

/// Used for debug-mode assertions
#define _ASSERT_DBG(_type_, _cond_) \
    if (!(_cond_)) { \
        LOG_ERROR(_type_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n", \
                  __LINE__, __FILE__, __TIME__); \
        if (!logger::AskYesNo("*** Assertion (see log)***\n")) logger::Crash(); \
    }

/// Used for message-specified debug-mode assertions
#define _ASSERT_DBG_MSG(_type_, _cond_, ...) \
    if (!(_cond_)) { \
        LOG_ERROR(_type_, __VA_ARGS__); \
        if (!logger::AskYesNo(__VA_ARGS__)) logger::Crash(); \
    }
#else
#define _ASSERT_DBG(_type_, _cond_, ...)
#define _ASSERT_DBG_MSG(_type_, _cond_, ...)
#define LOG_DEBUG(type, ...) 
#endif 

/// Used for general purpose assertions, CRITICAL operations only
#define _ASSERT_MSG(_type_, _cond_, ...) \
    if (!(_cond_)) { \
        if (!logger::AskYesNo(__VA_ARGS__)) logger::Crash(); \
    }

//////////////////////////////////////////////////////////////////////////////////////////////////// 
// Logger

namespace logger {

const int kMaxMsgLength = 1024; ///<  Maximum message length

/// Used for handling responses to system functions that require them
typedef enum {
    SYS_USER_NO = 0,    ///< User response for 'No'
    SYS_USER_YES,       ///< User response for 'Yes'
    SYS_USER_OK,        ///< User response for 'Okay'
    SYS_USER_ABORT,     ///< User response for 'Abort'
    SYS_USER_RETRY,     ///< User response for 'Retry'
    SYS_USER_CANCEL,    ///< User response for 'Cancel'
} SysUserResponse;

/// Level of logging
typedef enum {
    LNULL = 0,  ///< Logs with this level won't get logged
    LNOTICE,    ///< Notice: A general message to the user
    LERROR,     ///< Error: For failure messages
    LCRASH,     ///< Crash: Used for crash reports
    LWARNING,   ///< Warning: For potentially bad things, but not fatal
    LINFO,      ///< Info: Information message
    LDEBUG      ///< Debug: Debug-only information
} LogLevel;

/// Type of logging
typedef enum {
    TNULL = 0,
    TAI,
    TBOOT,
    TCOMMON,
    TCONFIG,
    TCORE,
    TCP,
    TDI,
    TDSP,
    TDVD,
    TEXI,
    TGP,
    THLE,
    THW,
    TJOYPAD,
    TMASTER,
    TMEM,
    TMI,
    TOS_HLE,
    TOS_REPORT,
    TPE,
    TPI,
    TPOWERPC,
    TSI,
    TVI,
    TVIDEO,
    NUMBER_OF_LOGS  ///< Number of logs - must be last
} LogType;

/// Used for implementing a logger for a subsystem
class LogContainer
{
public:
    LogContainer(const char* name, const char* desc, bool enable);
    ~LogContainer() {}

    const char* name() const { return name_; }
    const char* desc() const { return desc_; }

    bool enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }

    LogLevel level() const { return level_;	}
    void set_level(LogLevel level) { level_ = level; }

private:
    char name_[32];             ///< Name of the logger (e.g. "SI")
    char desc_[128];            ///< Description of the logger (e.g. "Serial Interface")
    bool enabled_;              ///< Whether or not the logger is enabled

    LogLevel    level_;         ///< Level of the logger (e.g. Notice, Error, Warning, etc.)

    SDL_mutex*  listener_lock_; ///< Mutex for multithreaded access

    DISALLOW_COPY_AND_ASSIGN(LogContainer);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Function Prototypes

/*! 
 * \brief Log routine used by everything
 * \param level Log level to use
 * \param type Log type to use
 * \param file Filename of file where error occured
 * \param line Linenumber of file where error occured
 * \param fmt Formatted message
 */
void LogGeneric(LogLevel level, LogType type, const char *file, int line, bool append, const char* fmt, ...);

/// Forces a controlled system crash rather before it catches fire (debug)
void Crash();

/*!
 * \brief Asks the user a yes or no question
 * \param fmt Question formatted message 
 * \return SysUserResponse response
 */
SysUserResponse AskYesNo(const char* fmt, ...);

/// Initialize the logging system
void Init();

} // namespace log

#endif // COMMON_LOG_H
