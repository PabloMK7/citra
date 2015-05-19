// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "generated/gl_3_2_core.h"

class OGLTexture : public NonCopyable {
public:
    OGLTexture();
    ~OGLTexture();

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle;
};

class OGLShader : public NonCopyable {
public:
    OGLShader();
    ~OGLShader();

    /// Creates a new internal OpenGL resource and stores the handle
    void Create(const char* vert_shader, const char* frag_shader);

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle;
};

class OGLBuffer : public NonCopyable {
public:
    OGLBuffer();
    ~OGLBuffer();

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle;
};

class OGLVertexArray : public NonCopyable {
public:
    OGLVertexArray();
    ~OGLVertexArray();

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle;
};

class OGLFramebuffer : public NonCopyable {
public:
    OGLFramebuffer();
    ~OGLFramebuffer();

    /// Creates a new internal OpenGL resource and stores the handle
    void Create();

    /// Deletes the internal OpenGL resource
    void Release();

    GLuint handle;
};
