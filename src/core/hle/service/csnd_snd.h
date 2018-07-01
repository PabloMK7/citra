// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CSND {

class CSND_SND final : public ServiceFramework<CSND_SND> {
public:
    CSND_SND();
    ~CSND_SND() = default;

private:
    /**
     * CSND_SND::Initialize service function
     *  Inputs:
     *      0 : Header Code[0x00010140]
     *      1 : Shared memory block size, for mem-block creation
     *      2 : Offset0 located in the shared-memory, region size=8
     *      3 : Offset1 located in the shared-memory, region size=12*num_channels
     *      4 : Offset2 located in the shared-memory, region size=8*num_capturedevices
     *      5 : Offset3 located in the shared-memory.
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Handle-list header
     *      3 : Mutex handle
     *      4 : Shared memory block handle
     */
    void Initialize(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::Shutdown service function
     *  Inputs:
     *      0 : Header Code[0x00020000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void Shutdown(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ExecuteCommands service function
     *  Inputs:
     *      0 : Header Code[0x00030040]
     *      1 : Command offset in shared memory.
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Available channel bit mask
     */
    void ExecuteCommands(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::AcquireSoundChannels service function
     *  Inputs:
     *      0 : Header Code[0x00050000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     *      2 : Available channel bit mask
     */
    void AcquireSoundChannels(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::ReleaseSoundChannels service function
     *  Inputs:
     *      0 : Header Code[0x00060000]
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void ReleaseSoundChannels(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::FlushDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x00090082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void FlushDataCache(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::StoreDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x000A0082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void StoreDataCache(Kernel::HLERequestContext& ctx);

    /**
     * CSND_SND::InvalidateDataCache service function
     *
     * This Function is a no-op, We aren't emulating the CPU cache any time soon.
     *
     *  Inputs:
     *      0 : Header Code[0x000B0082]
     *      1 : Address
     *      2 : Size
     *      3 : Value 0, some descriptor for the KProcess Handle
     *      4 : KProcess handle
     *  Outputs:
     *      1 : Result of function, 0 on success, otherwise error code
     */
    void InvalidateDataCache(Kernel::HLERequestContext& ctx);

    Kernel::SharedPtr<Kernel::Mutex> mutex = nullptr;
    Kernel::SharedPtr<Kernel::SharedMemory> shared_memory = nullptr;
};

/// Initializes the CSND_SND Service
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace CSND
} // namespace Service
