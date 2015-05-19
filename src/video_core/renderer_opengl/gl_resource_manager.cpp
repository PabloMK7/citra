// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"

// Textures
OGLTexture::OGLTexture() : handle(0) {
}

OGLTexture::~OGLTexture() {
    Release();
}

void OGLTexture::Create() {
    if (handle != 0) {
        return;
    }

    glGenTextures(1, &handle);
}

void OGLTexture::Release() {
    glDeleteTextures(1, &handle);
    handle = 0;
}

// Shaders
OGLShader::OGLShader() : handle(0) {
}

OGLShader::~OGLShader() {
    Release();
}

void OGLShader::Create(const char* vert_shader, const char* frag_shader) {
    if (handle != 0) {
        return;
    }

    handle = ShaderUtil::LoadShaders(vert_shader, frag_shader);
}

void OGLShader::Release() {
    glDeleteProgram(handle);
    handle = 0;
}

// Buffer objects
OGLBuffer::OGLBuffer() : handle(0) {
}

OGLBuffer::~OGLBuffer() {
    Release();
}

void OGLBuffer::Create() {
    if (handle != 0) {
        return;
    }

    glGenBuffers(1, &handle);
}

void OGLBuffer::Release() {
    glDeleteBuffers(1, &handle);
    handle = 0;
}

// Vertex array objects
OGLVertexArray::OGLVertexArray() : handle(0) {
}

OGLVertexArray::~OGLVertexArray() {
    Release();
}

void OGLVertexArray::Create() {
    if (handle != 0) {
        return;
    }

    glGenVertexArrays(1, &handle);
}

void OGLVertexArray::Release() {
    glDeleteVertexArrays(1, &handle);
    handle = 0;
}

// Framebuffers
OGLFramebuffer::OGLFramebuffer() : handle(0) {
}

OGLFramebuffer::~OGLFramebuffer() {
    Release();
}

void OGLFramebuffer::Create() {
    if (handle != 0) {
        return;
    }

    glGenFramebuffers(1, &handle);
}

void OGLFramebuffer::Release() {
    glDeleteFramebuffers(1, &handle);
    handle = 0;
}
