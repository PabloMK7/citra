// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <glad/glad.h>
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

class OGLStreamBuffer : private NonCopyable {
public:
    explicit OGLStreamBuffer(GLenum target);
    virtual ~OGLStreamBuffer() = default;

public:
    static std::unique_ptr<OGLStreamBuffer> MakeBuffer(bool storage_buffer, GLenum target);

    virtual void Create(size_t size, size_t sync_subdivide) = 0;
    virtual void Release() {}

    GLuint GetHandle() const;

    virtual std::pair<u8*, GLintptr> Map(size_t size, size_t alignment) = 0;
    virtual void Unmap() = 0;

protected:
    OGLBuffer gl_buffer;
    GLenum gl_target;

    size_t buffer_pos = 0;
    size_t buffer_size = 0;
    size_t buffer_sync_subdivide = 0;
    size_t mapped_size = 0;
};
