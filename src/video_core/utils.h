// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"

namespace FormatPrecision {

/// Adjust RGBA8 color with RGBA6 precision
static inline u32 rgba8_with_rgba6(u32 src) {
    u32 color = src;
    color &= 0xFCFCFCFC;
    color |= (color >> 6) & 0x03030303;
    return color;
}

/// Adjust RGBA8 color with RGB565 precision
static inline u32 rgba8_with_rgb565(u32 src) {
    u32 color = (src & 0xF8FCF8);
    color |= (color >> 5) & 0x070007;
    color |= (color >> 6) & 0x000300;
    color |= 0xFF000000;
    return color;
}

/// Adjust Z24 depth value with Z16 precision
static inline u32 z24_with_z16(u32 src) {
    return (src & 0xFFFF00) | (src >> 16);
}

} // namespace

namespace VideoCore {

/// Structure for the TGA texture format (for dumping)
struct TGAHeader {
    char  idlength;
    char  colourmaptype;
    char  datatypecode;
    short int colourmaporigin;
    short int colourmaplength;
    short int x_origin;
    short int y_origin;
    short width;
    short height;
    char  bitsperpixel;
    char  imagedescriptor;
};

/**
 * Dumps a texture to TGA
 * @param filename String filename to dump texture to
 * @param width Width of texture in pixels
 * @param height Height of texture in pixels
 * @param raw_data Raw RGBA8 texture data to dump
 * @todo This should be moved to some general purpose/common code
 */
void DumpTGA(std::string filename, short width, short height, u8* raw_data);

} // namespace
