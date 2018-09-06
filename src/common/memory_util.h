// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>

void* AllocateExecutableMemory(std::size_t size, bool low = true);
void* AllocateMemoryPages(std::size_t size);
void FreeMemoryPages(void* ptr, std::size_t size);
void* AllocateAlignedMemory(std::size_t size, std::size_t alignment);
void FreeAlignedMemory(void* ptr);
void WriteProtectMemory(void* ptr, std::size_t size, bool executable = false);
void UnWriteProtectMemory(void* ptr, std::size_t size, bool allowExecute = false);
std::string MemUsage();

inline int GetPageSize() {
    return 4096;
}
