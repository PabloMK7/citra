// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "cpu_detect.h"

namespace Common {

CPUInfo cpu_info;

CPUInfo::CPUInfo() { }

std::string CPUInfo::Summarize() {
    return "Generic";
}

} // namespace Common
