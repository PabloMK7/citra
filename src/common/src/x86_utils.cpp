/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    x86_utils.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-23
 * @brief   Utilities for the x86 architecture
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

#include "common.h"
#include "x86_utils.h"

#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define _interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define _interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#else

//#include <config/i386/cpuid.h>
#include <xmmintrin.h>

#if defined __FreeBSD__
#include <sys/types.h>
#include <machine/cpufunc.h>
#else
static inline void do_cpuid(unsigned int *eax, unsigned int *ebx,
    unsigned int *ecx, unsigned int *edx)
{
#ifdef _LP64
    // Note: EBX is reserved on Mac OS X and in PIC on Linux, so it has to
    // restored at the end of the asm block.
    __asm__ (
        "cpuid;"
        "movl  %%ebx,%1;"
        : "=a" (*eax),
        "=S" (*ebx),
        "=c" (*ecx),
        "=d" (*edx)
        : "a"  (*eax)
        : "rbx"
        );
#else
    __asm__ (
        "cpuid;"
        "movl  %%ebx,%1;"
        : "=a" (*eax),
        "=S" (*ebx),
        "=c" (*ecx),
        "=d" (*edx)
        : "a"  (*eax)
        : "ebx"
        );
#endif
}
#endif

static void __cpuid(int info[4], int x)
{
#if defined __FreeBSD__
    do_cpuid((unsigned int)x, (unsigned int*)info);
#else
    unsigned int eax = x, ebx = 0, ecx = 0, edx = 0;
    do_cpuid(&eax, &ebx, &ecx, &edx);
    info[0] = eax;
    info[1] = ebx;
    info[2] = ecx;
    info[3] = edx;
#endif
}

#endif

namespace common {

X86Utils::X86Utils() {
    memset(this, 0, sizeof(*this));
#ifdef _M_IX86

#elif defined (_M_X64)
    support_x64_os_ = true;
    support_sse_ = true;
    support_sse2_ = true;
#endif
    num_cores_ = 1;
#ifdef _WIN32
#ifdef _M_IX86
    int f64 = 0;
    IsWow64Process(GetCurrentProcess(), &f64);
    support_x64_os_ = (f64 == 1) ? true : false;
#endif
#endif
    // Assume CPU supports the CPUID instruction. Those that don't can barely
    // boot modern OS:es anyway.
    int cpu_id[4];
    char cpu_string[32];
    memset(cpu_string, 0, sizeof(cpu_string));

    // Detect CPU's CPUID capabilities, and grab cpu string
    __cpuid(cpu_id, 0x00000000);
    u32 max_std_fn = cpu_id[0];  // EAX
    *((int *)cpu_string) = cpu_id[1];
    *((int *)(cpu_string + 4)) = cpu_id[3];
    *((int *)(cpu_string + 8)) = cpu_id[2];
    __cpuid(cpu_id, 0x80000000);
    u32 max_ex_fn = cpu_id[0];
    if (!strcmp(cpu_string, "GenuineIntel")) {
        cpu_vendor_ = kVendorX86_Intel;
    } else if (!strcmp(cpu_string, "AuthenticAMD")) {
        cpu_vendor_ = kVendorX86_AMD;
    } else {
        cpu_vendor_ = kVendorX86_None;
    }

    // Detect family and other misc stuff.
    bool ht = false;
    support_hyper_threading_ = ht;
    logical_cpu_count_ = 1;
    if (max_std_fn >= 1) {
        __cpuid(cpu_id, 0x00000001);
        logical_cpu_count_ = (cpu_id[1] >> 16) & 0xFF;
        ht = (cpu_id[3] >> 28) & 1;

        if ((cpu_id[3] >> 25) & 1) support_sse_ = true;
        if ((cpu_id[3] >> 26) & 1) support_sse2_ = true;
        if ((cpu_id[2])       & 1) support_sse3_ = true;
        if ((cpu_id[2] >> 9)  & 1) support_ssse3_ = true;
        if ((cpu_id[2] >> 19) & 1) support_sse4_1_ = true;
        if ((cpu_id[2] >> 20) & 1) support_sse4_2_ = true;
    }
    if (max_ex_fn >= 0x80000004) {
        // Extract brand string
        __cpuid(cpu_id, 0x80000002);
//        memcpy(brand_string, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000003);
//        memcpy(brand_string + 16, cpu_id, sizeof(cpu_id));
        __cpuid(cpu_id, 0x80000004);
//        memcpy(brand_string + 32, cpu_id, sizeof(cpu_id));
    }
    num_cores_ = (logical_cpu_count_ == 0) ? 1 : logical_cpu_count_;

    if (max_ex_fn >= 0x80000008) {
        // Get number of cores. This is a bit complicated. Following AMD manual here.
        __cpuid(cpu_id, 0x80000008);
        int apic_id_core_id_size = (cpu_id[2] >> 12) & 0xF;
        if (apic_id_core_id_size == 0) {
            if (ht) {
                // New mechanism for modern Intel CPUs.
                if (cpu_vendor_ == kVendorX86_Intel) {
                    __cpuid(cpu_id, 0x00000004);
                    int cores_x_package = ((cpu_id[0] >> 26) & 0x3F) + 1;
                    support_hyper_threading_ = (cores_x_package < logical_cpu_count_);
                    cores_x_package = ((logical_cpu_count_ % cores_x_package) == 0) ? cores_x_package : 1;
                    num_cores_ = (cores_x_package > 1) ? cores_x_package : num_cores_;
                    logical_cpu_count_ /= cores_x_package;
                }
            }
        } else {
            // Use AMD's new method.
            num_cores_ = (cpu_id[2] & 0xFF) + 1;
        }
    }
    LOG_NOTICE(TCOMMON, "CPU detected (%s)", this->Summary().c_str());
}

X86Utils::~X86Utils() {
}

/**
 * Check if an X86 extension is supported by the current architecture
 * @param extension ExtensionX86 extension support to check for
 * @return True if the extension is supported, otherwise false
 */
bool X86Utils::IsExtensionSupported(X86Utils::ExtensionX86 extension) {
    switch (extension) {
    case kExtensionX86_SSE:
        return support_sse_;
    case kExtensionX86_SSE2:
        return support_sse2_;
    case kExtensionX86_SSE3:
        return support_sse3_;
    case kExtensionX86_SSSE3:
        return support_ssse3_;
    case kExtensionX86_SSE4_1:
        return support_sse4_1_;
    case kExtensionX86_SSE4_2:
        return support_sse4_2_;
    }
    return false;
}

/**
 * Gets a string summary of the X86 CPU information, suitable for printing
 * @return String summary
 */
std::string X86Utils::Summary() {
    const char* cpu_vendors[] = {
        "Unknown", "Intel", "AMD"
    };
    std::string res;
    res = FormatStr("%s, %d core%s", cpu_vendors[cpu_vendor_], num_cores_, (num_cores_ > 1) ? "s" : "");
    if (support_sse4_2_) {
        res += FormatStr(" (%i logical threads per physical core)", logical_cpu_count_);
    }
    if (support_sse_) res += ", SSE";
    if (support_sse2_) res += ", SSE2";
    if (support_sse3_) res += ", SSE3";
    if (support_ssse3_) res += ", SSSE3";
    if (support_sse4_1_) res += ", SSE4.1";
    if (support_sse4_2_) res += ", SSE4.2";
    if (support_hyper_threading_) res += ", HTT";
    //if (bLongMode) res += ", 64-bit support";
    return res;
}

} // namespace
