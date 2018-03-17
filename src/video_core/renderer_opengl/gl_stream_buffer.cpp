// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <deque>
#include <vector>
#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

class OrphanBuffer : public OGLStreamBuffer {
public:
    explicit OrphanBuffer(GLenum target) : OGLStreamBuffer(target) {}
    ~OrphanBuffer() override;

private:
    void Create(size_t size, size_t sync_subdivide) override;
    void Release() override;

    std::pair<u8*, GLintptr> Map(size_t size, size_t alignment) override;
    void Unmap() override;

    std::vector<u8> data;
};

class StorageBuffer : public OGLStreamBuffer {
public:
    explicit StorageBuffer(GLenum target) : OGLStreamBuffer(target) {}
    ~StorageBuffer() override;

private:
    void Create(size_t size, size_t sync_subdivide) override;
    void Release() override;

    std::pair<u8*, GLintptr> Map(size_t size, size_t alignment) override;
    void Unmap() override;

    struct Fence {
        OGLSync sync;
        size_t offset;
    };
    std::deque<Fence> head;
    std::deque<Fence> tail;

    u8* mapped_ptr;
};

OGLStreamBuffer::OGLStreamBuffer(GLenum target) {
    gl_target = target;
}

GLuint OGLStreamBuffer::GetHandle() const {
    return gl_buffer.handle;
}

std::unique_ptr<OGLStreamBuffer> OGLStreamBuffer::MakeBuffer(bool storage_buffer, GLenum target) {
    if (storage_buffer) {
        return std::make_unique<StorageBuffer>(target);
    }
    return std::make_unique<OrphanBuffer>(target);
}

OrphanBuffer::~OrphanBuffer() {
    Release();
}

void OrphanBuffer::Create(size_t size, size_t /*sync_subdivide*/) {
    buffer_pos = 0;
    buffer_size = size;
    data.resize(buffer_size);

    if (gl_buffer.handle == 0) {
        gl_buffer.Create();
        glBindBuffer(gl_target, gl_buffer.handle);
    }

    glBufferData(gl_target, static_cast<GLsizeiptr>(buffer_size), nullptr, GL_STREAM_DRAW);
}

void OrphanBuffer::Release() {
    gl_buffer.Release();
}

std::pair<u8*, GLintptr> OrphanBuffer::Map(size_t size, size_t alignment) {
    buffer_pos = Common::AlignUp(buffer_pos, alignment);

    if (buffer_pos + size > buffer_size) {
        Create(std::max(buffer_size, size), 0);
    }

    mapped_size = size;
    return std::make_pair(&data[buffer_pos], static_cast<GLintptr>(buffer_pos));
}

void OrphanBuffer::Unmap() {
    glBufferSubData(gl_target, static_cast<GLintptr>(buffer_pos),
                    static_cast<GLsizeiptr>(mapped_size), &data[buffer_pos]);
    buffer_pos += mapped_size;
}

StorageBuffer::~StorageBuffer() {
    Release();
}

void StorageBuffer::Create(size_t size, size_t sync_subdivide) {
    if (gl_buffer.handle != 0)
        return;

    buffer_pos = 0;
    buffer_size = size;
    buffer_sync_subdivide = std::max<size_t>(sync_subdivide, 1);

    gl_buffer.Create();
    glBindBuffer(gl_target, gl_buffer.handle);

    glBufferStorage(gl_target, static_cast<GLsizeiptr>(buffer_size), nullptr,
                    GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    mapped_ptr = reinterpret_cast<u8*>(
        glMapBufferRange(gl_target, 0, static_cast<GLsizeiptr>(buffer_size),
                         GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_FLUSH_EXPLICIT_BIT));
}

void StorageBuffer::Release() {
    if (gl_buffer.handle == 0)
        return;

    glUnmapBuffer(gl_target);

    gl_buffer.Release();
    head.clear();
    tail.clear();
}

std::pair<u8*, GLintptr> StorageBuffer::Map(size_t size, size_t alignment) {
    ASSERT(size <= buffer_size);

    OGLSync sync;

    buffer_pos = Common::AlignUp(buffer_pos, alignment);
    size_t effective_offset = Common::AlignDown(buffer_pos, buffer_sync_subdivide);

    if (!head.empty() &&
        (effective_offset > head.back().offset || buffer_pos + size > buffer_size)) {
        ASSERT(head.back().sync.handle == 0);
        head.back().sync.Create();
    }

    if (buffer_pos + size > buffer_size) {
        if (!tail.empty()) {
            std::swap(sync, tail.back().sync);
            tail.clear();
        }
        std::swap(tail, head);
        buffer_pos = 0;
        effective_offset = 0;
    }

    while (!tail.empty() && buffer_pos + size > tail.front().offset) {
        std::swap(sync, tail.front().sync);
        tail.pop_front();
    }

    if (sync.handle != 0) {
        glClientWaitSync(sync.handle, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
        sync.Release();
    }

    if (head.empty() || effective_offset > head.back().offset) {
        head.emplace_back();
        head.back().offset = effective_offset;
    }

    mapped_size = size;
    return std::make_pair(&mapped_ptr[buffer_pos], static_cast<GLintptr>(buffer_pos));
}

void StorageBuffer::Unmap() {
    glFlushMappedBufferRange(gl_target, static_cast<GLintptr>(buffer_pos),
                             static_cast<GLsizeiptr>(mapped_size));
    buffer_pos += mapped_size;
}
