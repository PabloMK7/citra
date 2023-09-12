// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <span>
#include <vector>
#include <tsl/robin_map.h>

#include "common/hash.h"
#include "video_core/renderer_vulkan/vk_common.h"

namespace Vulkan {

class Instance;

constexpr u32 MAX_DESCRIPTORS = 7;

union DescriptorData {
    vk::DescriptorImageInfo image_info;
    vk::DescriptorBufferInfo buffer_info;
    vk::BufferView buffer_view;

    bool operator==(const DescriptorData& other) const noexcept {
        return std::memcmp(this, &other, sizeof(DescriptorData)) == 0;
    }
};

using DescriptorSetData = std::array<DescriptorData, MAX_DESCRIPTORS>;

struct DataHasher {
    u64 operator()(const DescriptorSetData& data) const noexcept {
        return Common::ComputeHash64(data.data(), sizeof(data));
    }
};

/**
 * An interface for allocating descriptor sets that manages a collection of descriptor pools.
 */
class DescriptorPool {
public:
    explicit DescriptorPool(const Instance& instance);
    ~DescriptorPool();

    std::vector<vk::DescriptorSet> Allocate(vk::DescriptorSetLayout layout, u32 num_sets);

    vk::DescriptorSet Allocate(vk::DescriptorSetLayout layout);

private:
    vk::UniqueDescriptorPool CreatePool();

private:
    const Instance& instance;
    std::vector<vk::UniqueDescriptorPool> pools;
};

/**
 * Allocates and caches descriptor sets of a specific layout.
 */
class DescriptorSetProvider {
public:
    explicit DescriptorSetProvider(const Instance& instance, DescriptorPool& pool,
                                   std::span<const vk::DescriptorSetLayoutBinding> bindings);
    ~DescriptorSetProvider();

    vk::DescriptorSet Acquire(std::span<const DescriptorData> data);

    void FreeWithImage(vk::ImageView image_view);

    [[nodiscard]] vk::DescriptorSetLayout Layout() const noexcept {
        return *layout;
    }

    [[nodiscard]] vk::DescriptorSetLayout& Layout() noexcept {
        return layout.get();
    }

    [[nodiscard]] vk::DescriptorUpdateTemplate UpdateTemplate() const noexcept {
        return *update_template;
    }

private:
    DescriptorPool& pool;
    vk::Device device;
    vk::UniqueDescriptorSetLayout layout;
    vk::UniqueDescriptorUpdateTemplate update_template;
    std::vector<vk::DescriptorSet> free_sets;
    tsl::robin_map<DescriptorSetData, vk::DescriptorSet, DataHasher> descriptor_set_map;
};

} // namespace Vulkan
