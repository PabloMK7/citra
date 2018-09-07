// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <deque>
#include <vector>
#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

OGLStreamBuffer::OGLStreamBuffer(GLenum target, GLsizeiptr size, bool array_buffer_for_amd,
                                 bool prefer_coherent)
    : gl_target(target), buffer_size(size) {
    gl_buffer.Create();
    glBindBuffer(gl_target, gl_buffer.handle);

    GLsizeiptr allocate_size = size;
    if (array_buffer_for_amd) {
        // On AMD GPU there is a strange crash in indexed drawing. The crash happens when the buffer
        // read position is near the end and is an out-of-bound access to the vertex buffer. This is
        // probably a bug in the driver and is related to the usage of vec3<byte> attributes in the
        // vertex array. Doubling the allocation size for the vertex buffer seems to avoid the
        // crash.
        allocate_size *= 2;
    }

    if (GLAD_GL_ARB_buffer_storage) {
        persistent = true;
        coherent = prefer_coherent;
        GLbitfield flags =
            GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | (coherent ? GL_MAP_COHERENT_BIT : 0);
        glBufferStorage(gl_target, allocate_size, nullptr, flags);
        mapped_ptr = static_cast<u8*>(glMapBufferRange(
            gl_target, 0, buffer_size, flags | (coherent ? 0 : GL_MAP_FLUSH_EXPLICIT_BIT)));
    } else {
        glBufferData(gl_target, allocate_size, nullptr, GL_STREAM_DRAW);
    }
}

OGLStreamBuffer::~OGLStreamBuffer() {
    if (persistent) {
        glBindBuffer(gl_target, gl_buffer.handle);
        glUnmapBuffer(gl_target);
    }
    gl_buffer.Release();
}

GLuint OGLStreamBuffer::GetHandle() const {
    return gl_buffer.handle;
}

GLsizeiptr OGLStreamBuffer::GetSize() const {
    return buffer_size;
}

std::tuple<u8*, GLintptr, bool> OGLStreamBuffer::Map(GLsizeiptr size, GLintptr alignment) {
    ASSERT(size <= buffer_size);
    ASSERT(alignment <= buffer_size);
    mapped_size = size;

    if (alignment > 0) {
        buffer_pos = Common::AlignUp<std::size_t>(buffer_pos, alignment);
    }

    bool invalidate = false;
    if (buffer_pos + size > buffer_size) {
        buffer_pos = 0;
        invalidate = true;

        if (persistent) {
            glUnmapBuffer(gl_target);
        }
    }

    if (invalidate | !persistent) {
        GLbitfield flags = GL_MAP_WRITE_BIT | (persistent ? GL_MAP_PERSISTENT_BIT : 0) |
                           (coherent ? GL_MAP_COHERENT_BIT : GL_MAP_FLUSH_EXPLICIT_BIT) |
                           (invalidate ? GL_MAP_INVALIDATE_BUFFER_BIT : GL_MAP_UNSYNCHRONIZED_BIT);
        mapped_ptr = static_cast<u8*>(
            glMapBufferRange(gl_target, buffer_pos, buffer_size - buffer_pos, flags));
        mapped_offset = buffer_pos;
    }

    return std::make_tuple(mapped_ptr + buffer_pos - mapped_offset, buffer_pos, invalidate);
}

void OGLStreamBuffer::Unmap(GLsizeiptr size) {
    ASSERT(size <= mapped_size);

    if (!coherent && size > 0) {
        glFlushMappedBufferRange(gl_target, buffer_pos - mapped_offset, size);
    }

    if (!persistent) {
        glUnmapBuffer(gl_target);
    }

    buffer_pos += size;
}
