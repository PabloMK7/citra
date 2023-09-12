// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <QString>

/// Returns a list of all available vulkan GPUs.
std::vector<QString> GetVulkanPhysicalDevices();