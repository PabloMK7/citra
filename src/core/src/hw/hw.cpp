/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    hw.cpp
 * @author  bunnei
 * @date    2014-04-04
 * @brief   Hardware interface
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

#include "log.h"
#include "hw/hw.h"

namespace HW {

template <typename T>
inline void Read(T &var, const u32 addr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.

    // Could just do a base-relative read, too.... TODO

    //if ((addr & 0x3E000000) == 0x08000000) {
    //    var = *((const T*)&g_fcram[addr & MEM_FCRAM_MASK]);

    //} else {
    //    _assert_msg_(HW, false, "unknown hardware read");
    //}
}

template <typename T>
inline void Write(u32 addr, const T data) {
    //// ExeFS:/.code is loaded here:
    //if ((addr & 0xFFF00000) == 0x00100000) {
    //    // TODO(ShizZy): This is dumb... handle correctly. From 3DBrew:
    //    // http://3dbrew.org/wiki/Memory_layout#ARM11_User-land_memory_regions
    //    // The ExeFS:/.code is loaded here, executables must be loaded to the 0x00100000 region when
    //    // the exheader "special memory" flag is clear. The 0x03F00000-byte size restriction only 
    //    // applies when this flag is clear. Executables are usually loaded to 0x14000000 when the 
    //    // exheader "special memory" flag is set, however this address can be arbitrary.
    //    *(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;

    //// Error out...
    //} else {
    //    _assert_msg_(HW, false, "unknown hardware write");
    //}
}


void Init() {
}

void Shutdown() {
}

}