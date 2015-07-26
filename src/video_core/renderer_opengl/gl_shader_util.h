// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <glad/glad.h>

namespace ShaderUtil {

enum Attributes {
    ATTRIBUTE_POSITION  = 0,
    ATTRIBUTE_COLOR     = 1,
    ATTRIBUTE_TEXCOORDS = 2,
};

GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path);

}
