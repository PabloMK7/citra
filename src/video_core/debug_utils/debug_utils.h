// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "video_core/pica.h"

namespace Pica {

namespace DebugUtils {

// Simple utility class for dumping geometry data to an OBJ file
class GeometryDumper {
public:
    struct Vertex {
        std::array<float,3> pos;
    };

    void AddTriangle(Vertex& v0, Vertex& v1, Vertex& v2);

    void Dump();

private:
    struct Face {
        int index[3];
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
};

void DumpShader(const u32* binary_data, u32 binary_size, const u32* swizzle_data, u32 swizzle_size,
                u32 main_offset, const Regs::VSOutputAttributes* output_attributes);


// Utility class to log Pica commands.
struct PicaTrace {
    struct Write : public std::pair<u32,u32> {
        Write(u32 id, u32 value) : std::pair<u32,u32>(id, value) {}

        u32& Id() { return first; }
        const u32& Id() const { return first; }

        u32& Value() { return second; }
        const u32& Value() const { return second; }
    };
    std::vector<Write> writes;
};

void StartPicaTracing();
bool IsPicaTracing();
void OnPicaRegWrite(u32 id, u32 value);
std::unique_ptr<PicaTrace> FinishPicaTracing();

void DumpTexture(const Pica::Regs::TextureConfig& texture_config, u8* data);

void DumpTevStageConfig(const std::array<Pica::Regs::TevStageConfig,6>& stages);

} // namespace

} // namespace
