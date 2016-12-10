// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CSND {

class CSND_SND final : public Interface {
public:
    CSND_SND();

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

void Initialize(Interface* self);
void ExecuteType0Commands(Interface* self);
void AcquireSoundChannels(Interface* self);
void Shutdown(Interface* self);

} // namespace CSND
} // namespace Service
