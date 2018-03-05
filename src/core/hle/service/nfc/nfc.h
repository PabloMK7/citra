// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Kernel {
class Event;
} // namespace Kernel

namespace Service {
namespace NFC {

namespace ErrCodes {
enum {
    CommandInvalidForState = 512,
};
} // namespace ErrCodes

enum class TagState : u8 {
    NotInitialized = 0,
    NotScanning = 1,
    Scanning = 2,
    TagInRange = 3,
    TagOutOfRange = 4,
    TagDataLoaded = 5,
};

enum class CommunicationStatus : u8 {
    AttemptInitialize = 1,
    NfcInitialized = 2,
};

class Module final {
public:
    Module();
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session);
        ~Interface();

    protected:
        /**
         * NFC::Initialize service function
         *  Inputs:
         *      0 : Header code [0x00010040]
         *      1 : (u8) unknown parameter. Can be either value 0x1 or 0x2
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Initialize(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Shutdown service function
         *  Inputs:
         *      0 : Header code [0x00020040]
         *      1 : (u8) unknown parameter
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Shutdown(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StartCommunication service function
         *  Inputs:
         *      0 : Header code [0x00030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StartCommunication(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StopCommunication service function
         *  Inputs:
         *      0 : Header code [0x00040000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StopCommunication(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StartTagScanning service function
         *  Inputs:
         *      0 : Header code [0x00050040]
         *      1 : (u16) unknown. This is normally 0x0
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StartTagScanning(Kernel::HLERequestContext& ctx);

        /**
         * NFC::StopTagScanning service function
         *  Inputs:
         *      0 : Header code [0x00060000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void StopTagScanning(Kernel::HLERequestContext& ctx);

        /**
         * NFC::LoadAmiiboData service function
         *  Inputs:
         *      0 : Header code [0x00070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void LoadAmiiboData(Kernel::HLERequestContext& ctx);

        /**
         * NFC::ResetTagScanState service function
         *  Inputs:
         *      0 : Header code [0x00080000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ResetTagScanState(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagInRangeEvent service function
         *  Inputs:
         *      0 : Header code [0x000B0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Copy handle descriptor
         *      3 : Event Handle
         */
        void GetTagInRangeEvent(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagOutOfRangeEvent service function
         *  Inputs:
         *      0 : Header code [0x000C0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Copy handle descriptor
         *      3 : Event Handle
         */
        void GetTagOutOfRangeEvent(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagState service function
         *  Inputs:
         *      0 : Header code [0x000D0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : (u8) Tag state
         */
        void GetTagState(Kernel::HLERequestContext& ctx);

        /**
         * NFC::CommunicationGetStatus service function
         *  Inputs:
         *      0 : Header code [0x000F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : (u8) Communication state
         */
        void CommunicationGetStatus(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> nfc;
    };

private:
    Kernel::SharedPtr<Kernel::Event> tag_in_range_event;
    Kernel::SharedPtr<Kernel::Event> tag_out_of_range_event;
    TagState nfc_tag_state = TagState::NotInitialized;
    CommunicationStatus nfc_status = CommunicationStatus::NfcInitialized;
};

void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace NFC
} // namespace Service
