/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    utils.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-28
 * @brief   Utility functions (in general, not related to emulation) useful for video core
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

#include <stdio.h>
#include <string.h>

#include "utils.h"

namespace VideoCore {

/**
 * Dumps a texture to TGA
 * @param filename String filename to dump texture to
 * @param width Width of texture in pixels
 * @param height Height of texture in pixels
 * @param raw_data Raw RGBA8 texture data to dump
 * @todo This should be moved to some general purpose/common code
 */
void DumpTGA(std::string filename, int width, int height, u8* raw_data) {
    TGAHeader hdr;
    FILE* fout;
    u8 r, g, b;

    memset(&hdr, 0, sizeof(hdr));
    hdr.datatypecode = 2; // uncompressed RGB
    hdr.bitsperpixel = 24; // 24 bpp
    hdr.width = width;
    hdr.height = height;

    fout = fopen(filename.c_str(), "wb");
    fwrite(&hdr, sizeof(TGAHeader), 1, fout);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            r = raw_data[(4 * (i * width)) + (4 * j) + 0];
            g = raw_data[(4 * (i * width)) + (4 * j) + 1];
            b = raw_data[(4 * (i * width)) + (4 * j) + 2];
            putc(b, fout);
            putc(g, fout);
            putc(r, fout);
        }
    }
    fclose(fout);
}

} // namespace
