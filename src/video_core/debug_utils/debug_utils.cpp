// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <fstream>
#include <string>

#include "video_core/pica.h"

#include "debug_utils.h"

namespace Pica {

namespace DebugUtils {

void GeometryDumper::AddVertex(std::array<float,3> pos, TriangleTopology topology) {
    vertices.push_back({pos[0], pos[1], pos[2]});

    int num_vertices = vertices.size();

    switch (topology) {
    case TriangleTopology::List:
    case TriangleTopology::ListIndexed:
        if (0 == (num_vertices % 3))
            faces.push_back({ num_vertices-3, num_vertices-2, num_vertices-1 });
        break;

    default:
        ERROR_LOG(GPU, "Unknown triangle topology %x", (int)topology);
        exit(0);
        break;
    }
}

void GeometryDumper::Dump() {
    // NOTE: Permanently enabling this just trashes hard disks for no reason.
    //       Hence, this is currently disabled.
    return;

    static int index = 0;
    std::string filename = std::string("geometry_dump") + std::to_string(++index) + ".obj";

    std::ofstream file(filename);

    for (const auto& vertex : vertices) {
        file << "v " << vertex.pos[0]
             << " "  << vertex.pos[1]
             << " "  << vertex.pos[2] << std::endl;
    }

    for (const Face& face : faces) {
        file << "f " << 1+face.index[0]
             << " "  << 1+face.index[1]
             << " "  << 1+face.index[2] << std::endl;
    }
}

} // namespace

} // namespace
