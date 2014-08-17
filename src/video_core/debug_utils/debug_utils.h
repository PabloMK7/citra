// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <vector>

#include "video_core/pica.h"

namespace Pica {

namespace DebugUtils {

using TriangleTopology = Regs::TriangleTopology;

// Simple utility class for dumping geometry data to an OBJ file
class GeometryDumper {
public:
    void AddVertex(std::array<float,3> pos, TriangleTopology topology);

    void Dump();

private:
    struct Vertex {
        std::array<float,3> pos;
    };

    struct Face {
        int index[3];
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
};

} // namespace

} // namespace
