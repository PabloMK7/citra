// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/thread.h"
#ifdef __APPLE__
#include <mach/mach.h>
#elif defined(_WIN32)
#include <Windows.h>
#else
#if defined(BSD4_4) || defined(__OpenBSD__)
#include <pthread_np.h>
#else
#include <pthread.h>
#endif
#include <sched.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#endif

namespace Common {

int CurrentThreadId() {
#ifdef _MSC_VER
    return GetCurrentThreadId();
#elif defined __APPLE__
    return mach_thread_self();
#else
    return 0;
#endif
}

#ifdef _WIN32
// Supporting functions
void SleepCurrentThread(int ms) {
    Sleep(ms);
}
#endif

#ifdef _MSC_VER

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask) {
    SetThreadAffinityMask(thread, mask);
}

void SetCurrentThreadAffinity(u32 mask) {
    SetThreadAffinityMask(GetCurrentThread(), mask);
}

void SwitchCurrentThread() {
    SwitchToThread();
}

// Sets the debugger-visible name of the current thread.
// Uses undocumented (actually, it is now documented) trick.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vsdebug/html/vxtsksettingthreadname.asp

// This is implemented much nicer in upcoming msvc++, see:
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.100).aspx
void SetCurrentThreadName(const char* szThreadName) {
    static const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
    struct THREADNAME_INFO {
        DWORD dwType;     // must be 0x1000
        LPCSTR szName;    // pointer to name (in user addr space)
        DWORD dwThreadID; // thread ID (-1=caller thread)
        DWORD dwFlags;    // reserved for future use, must be zero
    } info;
#pragma pack(pop)

    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = -1; // dwThreadID;
    info.dwFlags = 0;

    __try {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_CONTINUE_EXECUTION) {
    }
}

#else // !MSVC_VER, so must be POSIX threads

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask) {
#ifdef __APPLE__
    thread_policy_set(pthread_mach_thread_np(thread), THREAD_AFFINITY_POLICY, (integer_t*)&mask, 1);
#elif (defined __linux__ || defined BSD4_4) && !(defined ANDROID)
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);

    for (int i = 0; i != sizeof(mask) * 8; ++i)
        if ((mask >> i) & 1)
            CPU_SET(i, &cpu_set);

    pthread_setaffinity_np(thread, sizeof(cpu_set), &cpu_set);
#endif
}

void SetCurrentThreadAffinity(u32 mask) {
    SetThreadAffinity(pthread_self(), mask);
}

#ifndef _WIN32
void SleepCurrentThread(int ms) {
    usleep(1000 * ms);
}

void SwitchCurrentThread() {
    usleep(1000 * 1);
}
#endif

// MinGW with the POSIX threading model does not support pthread_setname_np
#if !defined(_WIN32) || defined(_MSC_VER)
void SetCurrentThreadName(const char* szThreadName) {
#ifdef __APPLE__
    pthread_setname_np(szThreadName);
#elif defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), szThreadName);
#else
    pthread_setname_np(pthread_self(), szThreadName);
#endif
}
#endif

#endif

} // namespace Common
