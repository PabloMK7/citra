// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "video_core/pica.h"

#include "generated/gl_3_2_core.h"

namespace PicaToGL {

inline GLenum TextureFilterMode(Pica::Regs::TextureConfig::TextureFilter mode) {
    static const GLenum filter_mode_table[] = {
        GL_NEAREST,  // TextureFilter::Nearest
        GL_LINEAR    // TextureFilter::Linear
    };

    // Range check table for input
    if (mode >= ARRAY_SIZE(filter_mode_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture filtering mode %d", mode);
        UNREACHABLE();

        return GL_LINEAR;
    }

    GLenum gl_mode = filter_mode_table[mode];

    // Check for dummy values indicating an unknown mode
    if (gl_mode == 0) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture filtering mode %d", mode);
        UNIMPLEMENTED();

        return GL_LINEAR;
    }

    return gl_mode;
}

inline GLenum WrapMode(Pica::Regs::TextureConfig::WrapMode mode) {
    static const GLenum wrap_mode_table[] = {
        GL_CLAMP_TO_EDGE,  // WrapMode::ClampToEdge
        GL_CLAMP_TO_BORDER,// WrapMode::ClampToBorder
        GL_REPEAT,         // WrapMode::Repeat
        GL_MIRRORED_REPEAT // WrapMode::MirroredRepeat
    };

    // Range check table for input
    if (mode >= ARRAY_SIZE(wrap_mode_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture wrap mode %d", mode);
        UNREACHABLE();

        return GL_CLAMP_TO_EDGE;
    }

    GLenum gl_mode = wrap_mode_table[mode];

    // Check for dummy values indicating an unknown mode
    if (gl_mode == 0) {
        LOG_CRITICAL(Render_OpenGL, "Unknown texture wrap mode %d", mode);
        UNIMPLEMENTED();

        return GL_CLAMP_TO_EDGE;
    }

    return gl_mode;
}

inline GLenum BlendFunc(Pica::Regs::BlendFactor factor) {
    static const GLenum blend_func_table[] = {
        GL_ZERO,                     // BlendFactor::Zero
        GL_ONE,                      // BlendFactor::One
        GL_SRC_COLOR,                // BlendFactor::SourceColor
        GL_ONE_MINUS_SRC_COLOR,      // BlendFactor::OneMinusSourceColor
        GL_DST_COLOR,                // BlendFactor::DestColor
        GL_ONE_MINUS_DST_COLOR,      // BlendFactor::OneMinusDestColor
        GL_SRC_ALPHA,                // BlendFactor::SourceAlpha
        GL_ONE_MINUS_SRC_ALPHA,      // BlendFactor::OneMinusSourceAlpha
        GL_DST_ALPHA,                // BlendFactor::DestAlpha
        GL_ONE_MINUS_DST_ALPHA,      // BlendFactor::OneMinusDestAlpha
        GL_CONSTANT_COLOR,           // BlendFactor::ConstantColor
        GL_ONE_MINUS_CONSTANT_COLOR, // BlendFactor::OneMinusConstantColor
        GL_CONSTANT_ALPHA,           // BlendFactor::ConstantAlpha
        GL_ONE_MINUS_CONSTANT_ALPHA, // BlendFactor::OneMinusConstantAlpha
        GL_SRC_ALPHA_SATURATE,       // BlendFactor::SourceAlphaSaturate
    };

    // Range check table for input
    if ((unsigned)factor >= ARRAY_SIZE(blend_func_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown blend factor %d", factor);
        UNREACHABLE();

        return GL_ONE;
    }

    return blend_func_table[(unsigned)factor];
}

inline GLenum LogicOp(Pica::Regs::LogicOp op) {
    static const GLenum logic_op_table[] = {
        GL_CLEAR,           // Clear
        GL_AND,             // And
        GL_AND_REVERSE,     // AndReverse
        GL_COPY,            // Copy
        GL_SET,             // Set
        GL_COPY_INVERTED,   // CopyInverted
        GL_NOOP,            // NoOp
        GL_INVERT,          // Invert
        GL_NAND,            // Nand
        GL_OR,              // Or
        GL_NOR,             // Nor
        GL_XOR,             // Xor
        GL_EQUIV,           // Equiv
        GL_AND_INVERTED,    // AndInverted
        GL_OR_REVERSE,      // OrReverse
        GL_OR_INVERTED,     // OrInverted
    };

    // Range check table for input
    if ((unsigned)op >= ARRAY_SIZE(logic_op_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown logic op %d", op);
        UNREACHABLE();

        return GL_COPY;
    }

    return logic_op_table[(unsigned)op];
}

inline GLenum CompareFunc(Pica::Regs::CompareFunc func) {
    static const GLenum compare_func_table[] = {
        GL_NEVER,    // CompareFunc::Never
        GL_ALWAYS,   // CompareFunc::Always
        GL_EQUAL,    // CompareFunc::Equal
        GL_NOTEQUAL, // CompareFunc::NotEqual
        GL_LESS,     // CompareFunc::LessThan
        GL_LEQUAL,   // CompareFunc::LessThanOrEqual
        GL_GREATER,  // CompareFunc::GreaterThan
        GL_GEQUAL,   // CompareFunc::GreaterThanOrEqual
    };

    // Range check table for input
    if ((unsigned)func >= ARRAY_SIZE(compare_func_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown compare function %d", func);
        UNREACHABLE();

        return GL_ALWAYS;
    }

    return compare_func_table[(unsigned)func];
}

inline std::array<GLfloat, 4> ColorRGBA8(const u8* bytes) {
    return { { bytes[0] / 255.0f,
               bytes[1] / 255.0f,
               bytes[2] / 255.0f,
               bytes[3] / 255.0f
           } };
}

} // namespace
