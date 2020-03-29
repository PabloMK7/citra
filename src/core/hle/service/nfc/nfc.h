// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <boost/serialization/binary_object.hpp>
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class Event;
} // namespace Kernel

namespace Service::NFC {

namespace ErrCodes {
enum {
    CommandInvalidForState = 512,
};
} // namespace ErrCodes

// TODO(FearlessTobi): Add more members to this struct
struct AmiiboData {
    std::array<u8, 7> uuid;
    INSERT_PADDING_BYTES(0x4D);
    u16_le char_id;
    u8 char_variant;
    u8 figure_type;
    u16_be model_number;
    u8 series;
    INSERT_PADDING_BYTES(0x1C1);

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::make_binary_object(this, sizeof(AmiiboData));
    }
    friend class boost::serialization::access;
};
static_assert(sizeof(AmiiboData) == 0x21C, "AmiiboData is an invalid size");

enum class TagState : u8 {
    NotInitialized = 0,
    NotScanning = 1,
    Scanning = 2,
    TagInRange = 3,
    TagOutOfRange = 4,
    TagDataLoaded = 5,
    Unknown6 = 6,
};

enum class CommunicationStatus : u8 {
    AttemptInitialize = 1,
    NfcInitialized = 2,
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

        void LoadAmiibo(const AmiiboData& amiibo_data);

        void RemoveAmiibo();

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
         * NFC::GetAmiiboConfig service function
         *  Inputs:
         *      0 : Header code [0x00180000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-17 : 0x40-byte config struct
         */
        void GetAmiiboConfig(Kernel::HLERequestContext& ctx);

        /**
         * NFC::Unknown0x1A service function
         *  Inputs:
         *      0 : Header code [0x001A0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Unknown0x1A(Kernel::HLERequestContext& ctx);

        /**
         * NFC::GetIdentificationBlock service function
         *  Inputs:
         *      0 : Header code [0x001B0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *   2-31 : 0x36-byte struct
         */
        void GetIdentificationBlock(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> nfc;
    };

private:
    // Sync nfc_tag_state with amiibo_in_range and signal events on state change.
    void SyncTagState();

    std::shared_ptr<Kernel::Event> tag_in_range_event;
    std::shared_ptr<Kernel::Event> tag_out_of_range_event;
    TagState nfc_tag_state = TagState::NotInitialized;
    CommunicationStatus nfc_status = CommunicationStatus::NfcInitialized;

    AmiiboData amiibo_data{};
    bool amiibo_in_range = false;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int);
    friend class boost::serialization::access;
};

void InstallInterfaces(Core::System& system);

} // namespace Service::NFC

SERVICE_CONSTRUCT(Service::NFC::Module)
BOOST_CLASS_EXPORT_KEY(Service::NFC::Module)
