// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <optional>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_resource_pool.h"

namespace Vulkan {

ResourcePool::ResourcePool(MasterSemaphore* master_semaphore_, std::size_t grow_step_)
    : master_semaphore{master_semaphore_}, grow_step{grow_step_} {}

std::size_t ResourcePool::CommitResource() {
    // Refresh semaphore to query updated results
    master_semaphore->Refresh();
    const u64 gpu_tick = master_semaphore->KnownGpuTick();
    const auto search = [this, gpu_tick](std::size_t begin,
                                         std::size_t end) -> std::optional<std::size_t> {
        for (std::size_t iterator = begin; iterator < end; ++iterator) {
            if (gpu_tick >= ticks[iterator]) {
                ticks[iterator] = master_semaphore->CurrentTick();
                return iterator;
            }
        }
        return std::nullopt;
    };

    // Try to find a free resource from the hinted position to the end.
    std::optional<std::size_t> found = search(hint_iterator, ticks.size());
    if (!found) {
        // Search from beginning to the hinted position.
        found = search(0, hint_iterator);
        if (!found) {
            // Both searches failed, the pool is full; handle it.
            const std::size_t free_resource = ManageOverflow();

            ticks[free_resource] = master_semaphore->CurrentTick();
            found = free_resource;
        }
    }

    // Free iterator is hinted to the resource after the one that's been commited.
    hint_iterator = (*found + 1) % ticks.size();
    return *found;
}

std::size_t ResourcePool::ManageOverflow() {
    const std::size_t old_capacity = ticks.size();
    Grow();

    // The last entry is guaranted to be free, since it's the first element of the freshly
    // allocated resources.
    return old_capacity;
}

void ResourcePool::Grow() {
    const std::size_t old_capacity = ticks.size();
    ticks.resize(old_capacity + grow_step);
    Allocate(old_capacity, old_capacity + grow_step);
}

constexpr std::size_t COMMAND_BUFFER_POOL_SIZE = 4;

struct CommandPool::Pool {
    vk::CommandPool handle;
    std::array<vk::CommandBuffer, COMMAND_BUFFER_POOL_SIZE> cmdbufs;
};

CommandPool::CommandPool(const Instance& instance, MasterSemaphore* master_semaphore)
    : ResourcePool{master_semaphore, COMMAND_BUFFER_POOL_SIZE}, instance{instance} {}

CommandPool::~CommandPool() {
    vk::Device device = instance.GetDevice();
    for (Pool& pool : pools) {
        device.destroyCommandPool(pool.handle);
    }
}

void CommandPool::Allocate(std::size_t begin, std::size_t end) {
    // Command buffers are going to be commited, recorded, executed every single usage cycle.
    // They are also going to be reseted when commited.
    Pool& pool = pools.emplace_back();

    const vk::CommandPoolCreateInfo pool_create_info = {
        .flags = vk::CommandPoolCreateFlagBits::eTransient |
                 vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = instance.GetGraphicsQueueFamilyIndex(),
    };

    vk::Device device = instance.GetDevice();
    pool.handle = device.createCommandPool(pool_create_info);

    const vk::CommandBufferAllocateInfo buffer_alloc_info = {
        .commandPool = pool.handle,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = COMMAND_BUFFER_POOL_SIZE,
    };

    auto buffers = device.allocateCommandBuffers(buffer_alloc_info);
    std::copy(buffers.begin(), buffers.end(), pool.cmdbufs.begin());

    if (instance.HasDebuggingToolAttached()) {
        Vulkan::SetObjectName(device, pool.handle, "CommandPool: Pool({})",
                              COMMAND_BUFFER_POOL_SIZE);

        for (u32 i = 0; i < pool.cmdbufs.size(); ++i) {
            Vulkan::SetObjectName(device, pool.cmdbufs[i], "CommandPool: Command Buffer {}", i);
        }
    }
}

vk::CommandBuffer CommandPool::Commit() {
    const std::size_t index = CommitResource();
    const auto pool_index = index / COMMAND_BUFFER_POOL_SIZE;
    const auto sub_index = index % COMMAND_BUFFER_POOL_SIZE;
    return pools[pool_index].cmdbufs[sub_index];
}

} // namespace Vulkan
