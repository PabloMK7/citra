// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <boost/serialization/binary_object.hpp>
#include "common/common_types.h"
#include "core/hle/service/nfc/nfc_device.h"
#include "core/hle/service/nfc/nfc_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
} // namespace Kernel

namespace Service::NFC {

enum class CommunicationMode : u8 {
    NotInitialized = 0,
    Ntag = 1,
    Amiibo = 2,
    TrainTag = 3,
};

class Module final {
public:
    explicit Module(Core::System& system);
    ~Module();

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> nfc, const char* name, u32 max_session);
        ~Interface();

        std::shared_ptr<Module> GetModule() const;

        bool IsSearchingForAmiibos();

        bool IsTagActive();

        bool LoadAmiibo(const std::string& fullpath);

        void RemoveAmiibo();

    protected:
        /**
         * NFC::Initialize service function
         *  Inputs:
         *      0 : Header code [0x00010040]
         *      1 : (u8) CommunicationMode. Can be either value 0x1, 0x2 or 0x3
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Initialize(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Shutdown service function
         *  Inputs:
         *      0 : Header code [0x00020040]
         *      1 : (u8) CommunicationMode.
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
         * NFC::UpdateStoredAmiiboData service function
         *  Inputs:
         *      0 : Header code [0x00090002]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void UpdateStoredAmiiboData(Kernel::HLERequestContext& ctx);

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

        /**
         * NFC::GetTagInfo2 service function
         *  Inputs:
         *      0 : Header code [0x00100000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-26 : 0x60-byte struct
         */
        void GetTagInfo2(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagInfo service function
         *  Inputs:
         *      0 : Header code [0x00110000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-12 : 0x2C-byte struct
         */
        void GetTagInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetTagInfo service function
         *  Inputs:
         *      0 : Header code [0x00120000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Output NFC-adapter result-code
         */
        void CommunicationGetResult(Kernel::HLERequestContext& ctx);

        /**
         * NFC::OpenAppData service function
         *  Inputs:
         *      0 : Header code [0x00130040]
         *      1 : (u32) App ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void OpenAppData(Kernel::HLERequestContext& ctx);

        /**
         * NFC::InitializeWriteAppData service function
         *  Inputs:
         *      0 : Header code [0x00140384]
         *      1 : (u32) App ID
         *      2 : Size
         *   3-14 : 0x30-byte zeroed-out struct
         *     15 : 0x20, PID translate-header for kernel
         *     16 : PID written by kernel
         *     17 : (Size << 14) | 2
         *     18 : Pointer to input buffer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void InitializeWriteAppData(Kernel::HLERequestContext& ctx);

        /**
         * NFC::ReadAppData service function
         *  Inputs:
         *      0 : Header code [0x00150040]
         *      1 : Size (unused? Hard-coded to be 0xD8)
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ReadAppData(Kernel::HLERequestContext& ctx);

        /**
         * NFC::WriteAppData service function
         *  Inputs:
         *      0 : Header code [0x00160242]
         *      1 : Size
         *    2-9 : AmiiboWriteRequest struct (see above)
         *     10 : (Size << 14) | 2
         *     11 : Pointer to input appdata buffer
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void WriteAppData(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x00170000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-43 : AmiiboSettings struct (see above)
         */
        void GetRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetCommonInfo service function
         *  Inputs:
         *      0 : Header code [0x00180000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-17 : 0x40-byte config struct
         */
        void GetCommonInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetAppDataInitStruct service function
         *  Inputs:
         *      0 : Header code [0x00180000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-16 : 0x3C-byte config struct
         */
        void GetAppDataInitStruct(Kernel::HLERequestContext& ctx);

        /**
         * NFC::LoadAmiiboPartially service function
         *  Inputs:
         *      0 : Header code [0x001A0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void LoadAmiiboPartially(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetIdentificationBlock service function
         *  Inputs:
         *      0 : Header code [0x001B0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-31 : 0x36-byte struct
         */
        void GetIdentificationBlock(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Format service function
         *  Inputs:
         *      0 : Header code [0x040100C2]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Format(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetAdminInfo service function
         *  Inputs:
         *      0 : Header code [0x04020000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetAdminInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetEmptyRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04030000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void GetEmptyRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::SetRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04040A40]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void SetRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::DeleteRegisterInfo service function
         *  Inputs:
         *      0 : Header code [0x04050000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteRegisterInfo(Kernel::HLERequestContext& ctx);

        /**
         * NFC::DeleteApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x04060000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void DeleteApplicationArea(Kernel::HLERequestContext& ctx);

        /**
         * NFC::ExistsApplicationArea service function
         *  Inputs:
         *      0 : Header code [0x04070000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void ExistsApplicationArea(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> nfc;
    };

private:
    CommunicationMode nfc_mode = CommunicationMode::NotInitialized;

    std::shared_ptr<NfcDevice> device = nullptr;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NFC

SERVICE_CONSTRUCT(Service::NFC::Module)
BOOST_CLASS_EXPORT_KEY(Service::NFC::Module)
