// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"
#include "tests/core/arm/arm_test_common.h"

namespace ArmTests {

static std::shared_ptr<Memory::PageTable> page_table = nullptr;

TestEnvironment::TestEnvironment(bool mutable_memory_)
    : mutable_memory(mutable_memory_), test_memory(std::make_shared<TestMemory>(this)) {

    timing = std::make_unique<Core::Timing>(1, 100);
    memory = std::make_unique<Memory::MemorySystem>();
    kernel = std::make_unique<Kernel::KernelSystem>(*memory, *timing, [] {}, 0, 1, 0);

    kernel->SetCurrentProcess(kernel->CreateProcess(kernel->CreateCodeSet("", 0)));
    page_table = kernel->GetCurrentProcess()->vm_manager.page_table;

    page_table->Clear();

    memory->MapIoRegion(*page_table, 0x00000000, 0x80000000, test_memory);
    memory->MapIoRegion(*page_table, 0x80000000, 0x80000000, test_memory);

    kernel->SetCurrentMemoryPageTable(page_table);
}

TestEnvironment::~TestEnvironment() {
    memory->UnmapRegion(*page_table, 0x80000000, 0x80000000);
    memory->UnmapRegion(*page_table, 0x00000000, 0x80000000);
}

void TestEnvironment::SetMemory64(VAddr vaddr, u64 value) {
    SetMemory32(vaddr + 0, static_cast<u32>(value));
    SetMemory32(vaddr + 4, static_cast<u32>(value >> 32));
}

void TestEnvironment::SetMemory32(VAddr vaddr, u32 value) {
    SetMemory16(vaddr + 0, static_cast<u16>(value));
    SetMemory16(vaddr + 2, static_cast<u16>(value >> 16));
}

void TestEnvironment::SetMemory16(VAddr vaddr, u16 value) {
    SetMemory8(vaddr + 0, static_cast<u8>(value));
    SetMemory8(vaddr + 1, static_cast<u8>(value >> 8));
}

void TestEnvironment::SetMemory8(VAddr vaddr, u8 value) {
    test_memory->data[vaddr] = value;
}

std::vector<WriteRecord> TestEnvironment::GetWriteRecords() const {
    return write_records;
}

void TestEnvironment::ClearWriteRecords() {
    write_records.clear();
}

TestEnvironment::TestMemory::~TestMemory() {}

bool TestEnvironment::TestMemory::IsValidAddress(VAddr addr) {
    return true;
}

u8 TestEnvironment::TestMemory::Read8(VAddr addr) {
    auto iter = data.find(addr);
    if (iter == data.end()) {
        return addr; // Some arbitrary data
    }
    return iter->second;
}

u16 TestEnvironment::TestMemory::Read16(VAddr addr) {
    return Read8(addr) | static_cast<u16>(Read8(addr + 1)) << 8;
}

u32 TestEnvironment::TestMemory::Read32(VAddr addr) {
    return Read16(addr) | static_cast<u32>(Read16(addr + 2)) << 16;
}

u64 TestEnvironment::TestMemory::Read64(VAddr addr) {
    return Read32(addr) | static_cast<u64>(Read32(addr + 4)) << 32;
}

bool TestEnvironment::TestMemory::ReadBlock(VAddr src_addr, void* dest_buffer, std::size_t size) {
    VAddr addr = src_addr;
    u8* data = static_cast<u8*>(dest_buffer);

    for (std::size_t i = 0; i < size; i++, addr++, data++) {
        *data = Read8(addr);
    }

    return true;
}

void TestEnvironment::TestMemory::Write8(VAddr addr, u8 data) {
    env->write_records.emplace_back(8, addr, data);
    if (env->mutable_memory)
        env->SetMemory8(addr, data);
}

void TestEnvironment::TestMemory::Write16(VAddr addr, u16 data) {
    env->write_records.emplace_back(16, addr, data);
    if (env->mutable_memory)
        env->SetMemory16(addr, data);
}

void TestEnvironment::TestMemory::Write32(VAddr addr, u32 data) {
    env->write_records.emplace_back(32, addr, data);
    if (env->mutable_memory)
        env->SetMemory32(addr, data);
}

void TestEnvironment::TestMemory::Write64(VAddr addr, u64 data) {
    env->write_records.emplace_back(64, addr, data);
    if (env->mutable_memory)
        env->SetMemory64(addr, data);
}

bool TestEnvironment::TestMemory::WriteBlock(VAddr dest_addr, const void* src_buffer,
                                             std::size_t size) {
    VAddr addr = dest_addr;
    const u8* data = static_cast<const u8*>(src_buffer);

    for (std::size_t i = 0; i < size; i++, addr++, data++) {
        env->write_records.emplace_back(8, addr, *data);
        if (env->mutable_memory)
            env->SetMemory8(addr, *data);
    }

    return true;
}

} // namespace ArmTests
