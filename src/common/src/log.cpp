/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    log.cpp
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

#include <stdarg.h>
#include <stdlib.h>

#include "common.h"
#include "timer.h"

namespace logger {

LogContainer* g_logs[NUMBER_OF_LOGS]; ///< List of pointers to all logs

/// LogContainer constructor
LogContainer::LogContainer(const char* name, const char* desc, bool enable = false) {
	strncpy(name_, name, 128);
	strncpy(desc_, desc, 32);
	level_ = LWARNING;
}

/// Asks the user a yes or no question
SysUserResponse AskYesNo(const char* fmt, ...) {
    char    c;
    va_list arg;

    va_start(arg, fmt);
    printf("\n** Question **\n");
    vprintf(fmt, arg);
    va_end(arg);

    printf("  Response? (y/n) ");
    while (1) {
        c = getchar();
        if (c == 'y' || c == 'Y') {
            return SYS_USER_YES;
        }
        if (c == 'n' || c == 'N') {
            return SYS_USER_NO;
        }
    }
    return SYS_USER_NO;
}

//// Log routine used by everything
void LogGeneric(LogLevel level, LogType type, const char *file, int line, bool append, const char* fmt, ...)
{
    char msg[kMaxMsgLength];
    static const char level_to_char[8] = "-NECWID";
    static char last_char = '\n';
    static LogType last_type;
    
    va_list arg;
    va_start(arg, fmt);

    if (type >= NUMBER_OF_LOGS) {
        LOG_ERROR(TCOMMON, "Unknown logger type %d", type);
        return;
    }
    
    // Format the log message
    if (append) {
        sprintf(msg, "%s", fmt);
    } else {
        // char time_str[16];
        // u32 time_elapsed = common::GetTimeElapsed();
        // common::TicksToFormattedString(time_elapsed, time_str);
        sprintf(msg, "%c[%s] %s", level_to_char[(int)level], g_logs[type]->name(), fmt);
    }

    // If the last message didn't have a line break, print one
    if ('\n' != last_char && '\r' != last_char && !append && last_type != TOS_REPORT && 
        last_type != TOS_HLE) {
        printf("\n");
    }
    last_char = msg[strlen(msg)-1];
    last_type = type;

    // Print the log message to stdout
    vprintf(msg, arg);
    va_end(arg);
}

/// Forces a controlled system crash rather before it catches fire (debug)
void Crash() {
    LOG_CRASH(TCOMMON, "*** SYSTEM CRASHED ***\n");
    LOG_CRASH(TCOMMON, "Fatal error, system could not recover.\n");
#ifdef _MSC_VER
#ifdef USE_INLINE_ASM_X86
    __asm int 3
#endif
#elif defined(__GNUC__)
    asm("int $3");
#else
    LOG_CRASH(TCOMMON, "Exiting...\n");
    exit(0);
#endif
}

/// Initialize the logging system
void Init() {
    g_logs[TNULL]       = new LogContainer("NULL",      "Null");
    g_logs[TAI]         = new LogContainer("AI",        "AudioInterface");
    g_logs[TBOOT]       = new LogContainer("BOOT",      "Boot");
    g_logs[TCOMMON]     = new LogContainer("COMMON",    "Common");
    g_logs[TCONFIG]     = new LogContainer("CONFIG",    "Configuration");
    g_logs[TCORE]       = new LogContainer("CORE",      "SysCore");
    g_logs[TCP]         = new LogContainer("CP",        "CommandProcessor");
    g_logs[TDI]         = new LogContainer("DI",        "DVDInterface");
    g_logs[TDSP]        = new LogContainer("DSP",       "DSP");
    g_logs[TDVD]        = new LogContainer("DVD",       "GCM/ISO");
    g_logs[TEXI]        = new LogContainer("EXI",       "ExternalInterface");
    g_logs[TGP]         = new LogContainer("GP",        "GraphicsProcessor");
    g_logs[THLE]        = new LogContainer("HLE",       "HLE");
    g_logs[THW]         = new LogContainer("HW",        "Hardware");
    g_logs[TJOYPAD]     = new LogContainer("JOYPAD",    "Joypad");
    g_logs[TMASTER]     = new LogContainer("*",         "Master Log");
    g_logs[TMEM]        = new LogContainer("MEM",       "Memory");
    g_logs[TMI]         = new LogContainer("MI",        "MemoryInterface");
    g_logs[TOS_HLE]     = new LogContainer("OSHLE",     "OSHLE");
    g_logs[TOS_REPORT]  = new LogContainer("OSREPORT",  "OSREPORT");
    g_logs[TPE]         = new LogContainer("PE",        "PixelEngine");
    g_logs[TPI]         = new LogContainer("PI",        "ProcessorInterface");
    g_logs[TPOWERPC]    = new LogContainer("PPC",       "PowerPC");
    g_logs[TSI]         = new LogContainer("SI",        "SerialInterface");
    g_logs[TVI]         = new LogContainer("VI",        "VideoInterface");
    g_logs[TVIDEO]      = new LogContainer("VIDEO",     "VideoCore");

    LOG_NOTICE(TCOMMON, "%d logger(s) initalized ok", NUMBER_OF_LOGS);
}

} // namespace
