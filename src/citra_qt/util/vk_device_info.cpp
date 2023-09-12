// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/util/vk_device_info.h"
#include "video_core/renderer_vulkan/vk_instance.h"

std::vector<QString> GetVulkanPhysicalDevices() {
    std::vector<QString> result;
    try {
        Vulkan::Instance instance{};
        const auto physical_devices = instance.GetPhysicalDevices();

        for (const vk::PhysicalDevice physical_device : physical_devices) {
            const QString name = QString::fromUtf8(physical_device.getProperties().deviceName, -1);
            result.push_back(name);
        }
    } catch (const std::runtime_error& err) {
        LOG_ERROR(Frontend, "Error occured while querying for physical devices: {}", err.what());
    }

    return result;
}
