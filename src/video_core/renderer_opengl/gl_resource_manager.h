// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <utility>
#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state.h"

class OGLTexture : private NonCopyable {
public:
    OGLTexture() = default;

    OGLTexture(OGLTexture&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLTexture() {
        Release();
    }

    OGLTexture& operator=(OGLTexture&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create() {
        if (handle != 0)
            return;
        glGenTextures(1, &handle);
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteTextures(1, &handle);
        OpenGLState::GetCurState().ResetTexture(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLSampler : private NonCopyable {
public:
    OGLSampler() = default;

    OGLSampler(OGLSampler&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLSampler() {
        Release();
    }

    OGLSampler& operator=(OGLSampler&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create() {
        if (handle != 0)
            return;
        glGenSamplers(1, &handle);
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteSamplers(1, &handle);
        OpenGLState::GetCurState().ResetSampler(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLShader : private NonCopyable {
public:
    OGLShader() = default;

    OGLShader(OGLShader&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLShader() {
        Release();
    }

    OGLShader& operator=(OGLShader&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    void Create(const char* source, GLenum type) {
        if (handle != 0)
            return;
        if (source == nullptr)
            return;
        handle = GLShader::LoadShader(source, type);
    }

    void Release() {
        if (handle == 0)
            return;
        glDeleteShader(handle);
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLProgram : private NonCopyable {
public:
    OGLProgram() = default;

    OGLProgram(OGLProgram&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLProgram() {
        Release();
    }

    OGLProgram& operator=(OGLProgram&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new program from given shader objects
    void Create(bool separable_program, const std::vector<GLuint>& shaders) {
        if (handle != 0)
            return;
        handle = GLShader::LoadProgram(separable_program, shaders);
    }

    /// Creates a new program from given shader soruce code
    void Create(const char* vert_shader, const char* frag_shader) {
        OGLShader vert, frag;
        vert.Create(vert_shader, GL_VERTEX_SHADER);
        frag.Create(frag_shader, GL_FRAGMENT_SHADER);
        Create(false, {vert.handle, frag.handle});
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteProgram(handle);
        OpenGLState::GetCurState().ResetProgram(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLPipeline : private NonCopyable {
public:
    OGLPipeline() = default;
    OGLPipeline(OGLPipeline&& o) {
        handle = std::exchange<GLuint>(o.handle, 0);
    }
    ~OGLPipeline() {
        Release();
    }
    OGLPipeline& operator=(OGLPipeline&& o) {
        Release();
        handle = std::exchange<GLuint>(o.handle, 0);
        return *this;
    }

    void Create() {
        if (handle != 0)
            return;
        glGenProgramPipelines(1, &handle);
    }

    void Release() {
        if (handle == 0)
            return;
        glDeleteProgramPipelines(1, &handle);
        OpenGLState::GetCurState().ResetPipeline(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLBuffer : private NonCopyable {
public:
    OGLBuffer() = default;

    OGLBuffer(OGLBuffer&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLBuffer() {
        Release();
    }

    OGLBuffer& operator=(OGLBuffer&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create() {
        if (handle != 0)
            return;
        glGenBuffers(1, &handle);
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteBuffers(1, &handle);
        OpenGLState::GetCurState().ResetBuffer(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLVertexArray : private NonCopyable {
public:
    OGLVertexArray() = default;

    OGLVertexArray(OGLVertexArray&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLVertexArray() {
        Release();
    }

    OGLVertexArray& operator=(OGLVertexArray&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create() {
        if (handle != 0)
            return;
        glGenVertexArrays(1, &handle);
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteVertexArrays(1, &handle);
        OpenGLState::GetCurState().ResetVertexArray(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};

class OGLFramebuffer : private NonCopyable {
public:
    OGLFramebuffer() = default;

    OGLFramebuffer(OGLFramebuffer&& o) : handle(std::exchange(o.handle, 0)) {}

    ~OGLFramebuffer() {
        Release();
    }

    OGLFramebuffer& operator=(OGLFramebuffer&& o) {
        Release();
        handle = std::exchange(o.handle, 0);
        return *this;
    }

    /// Creates a new internal OpenGL resource and stores the handle
    void Create() {
        if (handle != 0)
            return;
        glGenFramebuffers(1, &handle);
    }

    /// Deletes the internal OpenGL resource
    void Release() {
        if (handle == 0)
            return;
        glDeleteFramebuffers(1, &handle);
        OpenGLState::GetCurState().ResetFramebuffer(handle).Apply();
        handle = 0;
    }

    GLuint handle = 0;
};
