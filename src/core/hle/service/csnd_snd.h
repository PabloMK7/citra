// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace CSND_SND

namespace CSND_SND {

class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "csnd:SND";
    }
};

struct Type0Command {
    // command id and next command offset
    u32 command_id;
    u32 finished;
    u32 flags;
    u8 parameters[20];
};

void Initialize(Service::Interface* self);
void ExecuteType0Commands(Service::Interface* self);
void AcquireSoundChannels(Service::Interface* self);
void Shutdown(Service::Interface* self);

} // namespace
