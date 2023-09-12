// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_instance.h"

namespace Vulkan {

MICROPROFILE_DEFINE(Vulkan_DescriptorSetAcquire, "Vulkan", "Descriptor Set Acquire",
                    MP_RGB(64, 128, 256));

constexpr u32 MAX_BATCH_SIZE = 8;

DescriptorPool::DescriptorPool(const Instance& instance_) : instance{instance_} {
    auto& pool = pools.emplace_back();
    pool = CreatePool();
}

DescriptorPool::~DescriptorPool() = default;

std::vector<vk::DescriptorSet> DescriptorPool::Allocate(vk::DescriptorSetLayout layout,
                                                        u32 num_sets) {
    std::array<vk::DescriptorSetLayout, MAX_BATCH_SIZE> layouts;
    layouts.fill(layout);

    u32 current_pool = 0;
    vk::DescriptorSetAllocateInfo alloc_info = {
        .descriptorPool = *pools[current_pool],
        .descriptorSetCount = num_sets,
        .pSetLayouts = layouts.data(),
    };

    while (true) {
        try {
            return instance.GetDevice().allocateDescriptorSets(alloc_info);
        } catch (const vk::OutOfPoolMemoryError&) {
            current_pool++;
            if (current_pool == pools.size()) {
                LOG_INFO(Render_Vulkan, "Run out of pools, creating new one!");
                auto& pool = pools.emplace_back();
                pool = CreatePool();
            }
            alloc_info.descriptorPool = *pools[current_pool];
        }
    }
}

vk::DescriptorSet DescriptorPool::Allocate(vk::DescriptorSetLayout layout) {
    const auto sets = Allocate(layout, 1);
    return sets[0];
}

vk::UniqueDescriptorPool DescriptorPool::CreatePool() {
    // Choose a sane pool size good for most games
    static constexpr std::array<vk::DescriptorPoolSize, 6> pool_sizes = {{
        {vk::DescriptorType::eUniformBufferDynamic, 64},
        {vk::DescriptorType::eUniformTexelBuffer, 64},
        {vk::DescriptorType::eCombinedImageSampler, 4096},
        {vk::DescriptorType::eSampledImage, 256},
        {vk::DescriptorType::eStorageImage, 256},
        {vk::DescriptorType::eStorageBuffer, 32},
    }};

    const vk::DescriptorPoolCreateInfo descriptor_pool_info = {
        .maxSets = 4098,
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    return instance.GetDevice().createDescriptorPoolUnique(descriptor_pool_info);
}

DescriptorSetProvider::DescriptorSetProvider(
    const Instance& instance, DescriptorPool& pool_,
    std::span<const vk::DescriptorSetLayoutBinding> bindings)
    : pool{pool_}, device{instance.GetDevice()} {
    std::array<vk::DescriptorUpdateTemplateEntry, MAX_DESCRIPTORS> update_entries;

    for (u32 i = 0; i < bindings.size(); i++) {
        update_entries[i] = vk::DescriptorUpdateTemplateEntry{
            .dstBinding = bindings[i].binding,
            .dstArrayElement = 0,
            .descriptorCount = bindings[i].descriptorCount,
            .descriptorType = bindings[i].descriptorType,
            .offset = i * sizeof(DescriptorData),
            .stride = sizeof(DescriptorData),
        };
    }

    const vk::DescriptorSetLayoutCreateInfo layout_info = {
        .bindingCount = static_cast<u32>(bindings.size()),
        .pBindings = bindings.data(),
    };
    layout = device.createDescriptorSetLayoutUnique(layout_info);

    const vk::DescriptorUpdateTemplateCreateInfo template_info = {
        .descriptorUpdateEntryCount = static_cast<u32>(bindings.size()),
        .pDescriptorUpdateEntries = update_entries.data(),
        .templateType = vk::DescriptorUpdateTemplateType::eDescriptorSet,
        .descriptorSetLayout = *layout,
    };
    update_template = device.createDescriptorUpdateTemplateUnique(template_info);
}

DescriptorSetProvider::~DescriptorSetProvider() = default;

vk::DescriptorSet DescriptorSetProvider::Acquire(std::span<const DescriptorData> data) {
    MICROPROFILE_SCOPE(Vulkan_DescriptorSetAcquire);
    DescriptorSetData key{};
    std::memcpy(key.data(), data.data(), data.size_bytes());
    const auto [it, new_set] = descriptor_set_map.try_emplace(key);
    if (!new_set) {
        return it->second;
    }
    if (free_sets.empty()) {
        free_sets = pool.Allocate(*layout, MAX_BATCH_SIZE);
    }
    it.value() = free_sets.back();
    free_sets.pop_back();
    device.updateDescriptorSetWithTemplate(it->second, *update_template, data[0]);
    return it->second;
}

void DescriptorSetProvider::FreeWithImage(vk::ImageView image_view) {
    for (auto it = descriptor_set_map.begin(); it != descriptor_set_map.end();) {
        const auto& [data, set] = *it;
        const bool has_image = std::any_of(data.begin(), data.end(), [image_view](auto& info) {
            return info.image_info.imageView == image_view;
        });
        if (has_image) {
            free_sets.push_back(set);
            it = descriptor_set_map.erase(it);
        } else {
            it++;
        }
    }
}

} // namespace Vulkan
