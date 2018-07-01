// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/event.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

enum class CecStateAbbreviated {
    CEC_STATE_ABBREV_IDLE = 1,      ///< Corresponds to CEC_STATE_IDLE
    CEC_STATE_ABBREV_NOT_LOCAL = 2, ///< Corresponds to CEC_STATEs *FINISH*, *POST, and OVER_BOSS
    CEC_STATE_ABBREV_SCANNING = 3,  ///< Corresponds to CEC_STATE_SCANNING
    CEC_STATE_ABBREV_WLREADY =
        4, ///< Corresponds to CEC_STATE_WIRELESS_READY when some unknown bool is true
    CEC_STATE_ABBREV_OTHER = 5, ///< Corresponds to CEC_STATEs besides *FINISH*, *POST, and
                                /// OVER_BOSS and those listed here
};

class Module final {
public:
    Module();
    ~Module() = default;

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * GetCecStateAbbreviated service function
         *  Inputs:
         *      0: 0x000E0000
         *  Outputs:
         *      1: ResultCode
         *      2: CecStateAbbreviated
         */
        void GetCecStateAbbreviated(Kernel::HLERequestContext& ctx);

        /**
         * GetCecInfoEventHandle service function
         *  Inputs:
         *      0: 0x000F0000
         *  Outputs:
         *      1: ResultCode
         *      3: Event Handle
         */
        void GetCecInfoEventHandle(Kernel::HLERequestContext& ctx);

        /**
         * GetChangeStateEventHandle service function
         *  Inputs:
         *      0: 0x00100000
         *  Outputs:
         *      1: ResultCode
         *      3: Event Handle
         */
        void GetChangeStateEventHandle(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> cecd;
    };

private:
    Kernel::SharedPtr<Kernel::Event> cecinfo_event;
    Kernel::SharedPtr<Kernel::Event> change_state_event;
};

/// Initialize CECD service(s)
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace CECD
} // namespace Service
