// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"

#include "video_core/pica.h"

using GLvec2 = std::array<GLfloat, 2>;
using GLvec3 = std::array<GLfloat, 3>;
using GLvec4 = std::array<GLfloat, 4>;

namespace PicaToGL {

inline GLenum TextureFilterMode(Pica::Regs::TextureConfig::TextureFilter mode) {
    static const GLenum filter_mode_table[] = {
        GL_NEAREST,  // TextureFilter::Nearest
        GL_LINEAR    // TextureFilter::Linear
    };

    // Range check table for input
    if (static_cast<size_t>(mode) >= ARRAY_SIZE(filter_mode_table)) {
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
    if (static_cast<size_t>(mode) >= ARRAY_SIZE(wrap_mode_table)) {
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

inline GLenum BlendEquation(Pica::Regs::BlendEquation equation) {
    static const GLenum blend_equation_table[] = {
        GL_FUNC_ADD,              // BlendEquation::Add
        GL_FUNC_SUBTRACT,         // BlendEquation::Subtract
        GL_FUNC_REVERSE_SUBTRACT, // BlendEquation::ReverseSubtract
        GL_MIN,                   // BlendEquation::Min
        GL_MAX,                   // BlendEquation::Max
    };

    // Range check table for input
    if (static_cast<size_t>(equation) >= ARRAY_SIZE(blend_equation_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown blend equation %d", equation);
        UNREACHABLE();

        return GL_FUNC_ADD;
    }

    return blend_equation_table[(unsigned)equation];
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
    if (static_cast<size_t>(factor) >= ARRAY_SIZE(blend_func_table)) {
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
    if (static_cast<size_t>(op) >= ARRAY_SIZE(logic_op_table)) {
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
    if (static_cast<size_t>(func) >= ARRAY_SIZE(compare_func_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown compare function %d", func);
        UNREACHABLE();

        return GL_ALWAYS;
    }

    return compare_func_table[(unsigned)func];
}

inline GLenum StencilOp(Pica::Regs::StencilAction action) {
    static const GLenum stencil_op_table[] = {
        GL_KEEP,        // StencilAction::Keep
        GL_ZERO,        // StencilAction::Zero
        GL_REPLACE,     // StencilAction::Replace
        GL_INCR,        // StencilAction::Increment
        GL_DECR,        // StencilAction::Decrement
        GL_INVERT,      // StencilAction::Invert
        GL_INCR_WRAP,   // StencilAction::IncrementWrap
        GL_DECR_WRAP    // StencilAction::DecrementWrap
    };

    // Range check table for input
    if (static_cast<size_t>(action) >= ARRAY_SIZE(stencil_op_table)) {
        LOG_CRITICAL(Render_OpenGL, "Unknown stencil op %d", action);
        UNREACHABLE();

        return GL_KEEP;
    }

    return stencil_op_table[(unsigned)action];
}

inline GLvec4 ColorRGBA8(const u32 color) {
    return { { (color >>  0 & 0xFF) / 255.0f,
               (color >>  8 & 0xFF) / 255.0f,
               (color >> 16 & 0xFF) / 255.0f,
               (color >> 24 & 0xFF) / 255.0f
           } };
}

inline std::array<GLfloat, 3> LightColor(const Pica::Regs::LightColor& color) {
    return { { color.r / 255.0f,
               color.g / 255.0f,
               color.b / 255.0f
           } };
}

} // namespace
