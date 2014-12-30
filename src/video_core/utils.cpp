// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <stdio.h>
#include <string.h>

#include "video_core/utils.h"

namespace VideoCore {

/**
 * Dumps a texture to TGA
 * @param filename String filename to dump texture to
 * @param width Width of texture in pixels
 * @param height Height of texture in pixels
 * @param raw_data Raw RGBA8 texture data to dump
 * @todo This should be moved to some general purpose/common code
 */
void DumpTGA(std::string filename, short width, short height, u8* raw_data) {
    TGAHeader hdr = {0, 0, 2, 0, 0, 0, 0, width, height, 24, 0};
    FILE* fout = fopen(filename.c_str(), "wb");

    fwrite(&hdr, sizeof(TGAHeader), 1, fout);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            putc(raw_data[(3 * (y * width)) + (3 * x) + 0], fout); // b
            putc(raw_data[(3 * (y * width)) + (3 * x) + 1], fout); // g
            putc(raw_data[(3 * (y * width)) + (3 * x) + 2], fout); // r
        }
    }

    fclose(fout);
}
} // namespace
