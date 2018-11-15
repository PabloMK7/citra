// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

Object::Object(KernelSystem& kernel) : object_id{kernel.GenerateObjectID()} {}

Object::~Object() = default;

bool Object::IsWaitable() const {
    switch (GetHandleType()) {
    case HandleType::Event:
    case HandleType::Mutex:
    case HandleType::Thread:
    case HandleType::Semaphore:
    case HandleType::Timer:
    case HandleType::ServerPort:
    case HandleType::ServerSession:
        return true;

    case HandleType::Unknown:
    case HandleType::SharedMemory:
    case HandleType::Process:
    case HandleType::AddressArbiter:
    case HandleType::ResourceLimit:
    case HandleType::CodeSet:
    case HandleType::ClientPort:
    case HandleType::ClientSession:
        return false;
    }

    UNREACHABLE();
}

} // namespace Kernel
