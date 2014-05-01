// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
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
                b = raw_data[(3 * (i * width)) + (3 * j) + 0];
                g = raw_data[(3 * (i * width)) + (3 * j) + 1];
                r = raw_data[(3 * (i * width)) + (3 * j) + 2];
                putc(b, fout);
                putc(g, fout);
                putc(r, fout);
            }
        }
        fclose(fout);
    }
} // namespace