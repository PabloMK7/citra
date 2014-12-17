// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/msg_handler.h"
#include "common/logging/log.h"

#ifdef _WIN32
#ifndef __func__
#define __func__ __FUNCTION__
#endif
#endif

#if _DEBUG
#define _dbg_assert_(_t_, _a_) \
    if (!(_a_)) {\
        LOG_CRITICAL(_t_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
                       __LINE__, __FILE__, __TIME__); \
        if (!PanicYesNo("*** Assertion (see log)***\n")) {Crash();} \
    }
#define _dbg_assert_msg_(_t_, _a_, ...)\
    if (!(_a_)) {\
        LOG_CRITICAL(_t_, __VA_ARGS__); \
        if (!PanicYesNo(__VA_ARGS__)) {Crash();} \
    }
#define _dbg_update_() Host_UpdateLogDisplay();

#else // not debug
#define _dbg_update_() ;

#ifndef _dbg_assert_
#define _dbg_assert_(_t_, _a_) {}
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) {}
#endif // dbg_assert
#endif

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)

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
