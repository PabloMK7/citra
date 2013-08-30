/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    x86_utils.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
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

#ifndef COMMON_X86_UTILS_
#define COMMON_X86_UTILS_

#include <string>
#include "types.h"

// Common namespace
namespace common {

class X86Utils {
public:
    /// Enumeration of X86 vendors
    enum VendorX86 {
        kVendorX86_None = 0,
        kVendorX86_Intel,
        kVendorX86_AMD,
        kVendorX86_NumberOf
    };

    /// Enumeration of X86 extensions
    enum ExtensionX86 {
        kExtensionX86_None = 0,
        kExtensionX86_SSE,
        kExtensionX86_SSE2,
        kExtensionX86_SSE3,
        kExtensionX86_SSSE3,
        kExtensionX86_SSE4_1,
        kExtensionX86_SSE4_2,
        kExtensionX86_NumberOf
    };

    X86Utils();
    ~X86Utils();

    /**
     * Check if an X86 extension is supported by the current architecture
     * @param extension ExtensionX86 extension support to check for
     * @return True if the extension is supported, otherwise false
     */
    bool IsExtensionSupported(ExtensionX86 extension);
    
    /**
     * Gets a string summary of the X86 CPU information, suitable for printing
     * @return String summary
     */
    std::string Summary();

private:
    bool support_x64_os_;
    bool support_x64_cpu_;
    bool support_hyper_threading_;
    
    int num_cores_;
    int logical_cpu_count_;

    bool support_sse_;
    bool support_sse2_;
    bool support_sse3_;
    bool support_ssse3_;
    bool support_sse4_1_;
    bool support_sse4_2_;

    VendorX86 cpu_vendor_;
};

} // namespace

#endif
