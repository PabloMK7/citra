// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/kernel/event.h"
#include "core/hle/service/service.h"
#include "core/hle/service/fs/archive.h"

namespace Service {
namespace CECD {

class Module final {
public:
    Module();
    ~Module();

    enum class CecStateAbbreviated : u32 {
        CEC_STATE_ABBREV_IDLE = 1,      /// Relates to CEC_STATE_IDLE
        CEC_STATE_ABBREV_NOT_LOCAL = 2, /// Relates to CEC_STATEs *FINISH*, *POST, and OVER_BOSS
        CEC_STATE_ABBREV_SCANNING = 3,  /// Relates to CEC_STATE_SCANNING
        CEC_STATE_ABBREV_WLREADY = 4,   /// Relates to CEC_STATE_WIRELESS_READY when a bool is true
        CEC_STATE_ABBREV_OTHER = 5,     /// Relates to CEC_STATEs besides *FINISH*, *POST, and
    };                                  /// OVER_BOSS and those listed here

    enum class CecCommand : u32 {
        CEC_COMMAND_NONE = 0,
        CEC_COMMAND_START = 1,
        CEC_COMMAND_RESET_START = 2,
        CEC_COMMAND_READYSCAN = 3,
        CEC_COMMAND_READYSCANWAIT = 4,
        CEC_COMMAND_STARTSCAN = 5,
        CEC_COMMAND_RESCAN = 6,
        CEC_COMMAND_NDM_RESUME = 7,
        CEC_COMMAND_NDM_SUSPEND = 8,
        CEC_COMMAND_NDM_SUSPEND_IMMEDIATE = 9,
        CEC_COMMAND_STOPWAIT = 0x0A,
        CEC_COMMAND_STOP = 0x0B,
        CEC_COMMAND_STOP_FORCE = 0x0C,
        CEC_COMMAND_STOP_FORCE_WAIT = 0x0D,
        CEC_COMMAND_RESET_FILTER = 0x0E,
        CEC_COMMAND_DAEMON_STOP = 0x0F,
        CEC_COMMAND_DAEMON_START = 0x10,
        CEC_COMMAND_EXIT = 0x11,
        CEC_COMMAND_OVER_BOSS = 0x12,
        CEC_COMMAND_OVER_BOSS_FORCE = 0x13,
        CEC_COMMAND_OVER_BOSS_FORCE_WAIT = 0x14,
        CEC_COMMAND_END = 0x15,
    };

    enum class CecDataPathType : u32 {
        CEC_PATH_MBOX_LIST = 1,
        CEC_PATH_MBOX_INFO = 2,
        CEC_PATH_INBOX_INFO = 3,
        CEC_PATH_OUTBOX_INFO = 4,
        CEC_PATH_OUTBOX_INDEX = 5,
        CEC_PATH_INBOX_MSG = 6,
        CEC_PATH_OUTBOX_MSG = 7,
        CEC_PATH_ROOT_DIR = 10,
        CEC_PATH_MBOX_DIR = 11,
        CEC_PATH_INBOX_DIR = 12,
        CEC_PATH_OUTBOX_DIR = 13,
        CEC_MBOX_DATA = 100,
        CEC_MBOX_ICON = 101,
        CEC_MBOX_TITLE = 110,
    };

    class Interface : public ServiceFramework<Interface> {
    public:
        Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * CECD::OpenRawFile service function
         *  Inputs:
         *      0 : Header Code[0x000100C2]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : File open flag
         *      4 : Descriptor for process ID
         *      5 : Placeholder for process ID
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : File size?
         */
        void OpenRawFile(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadRawFile service function
         *  Inputs:
         *      0 : Header Code[0x00020042]
         *      1 : Buffer size (unused)
         *      2 : Descriptor for mapping a write-only buffer in the target process
         *      3 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Buffer address
         */
        void ReadRawFile(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadMessage service function
         *  Inputs:
         *      0 : Header Code[0x00030104]
         *      1 : NCCH Program ID
         *      2 : bool is_out_box?
         *      3 : Message ID size (unused, always 8)
         *      4 : Buffer size (unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a read-only buffer in the target process
         *      4 : Message ID address
         *      5 : Descriptor for mapping a write-only buffer in the target process
         *      6 : Buffer address
         */
        void ReadMessage(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadMessageWithHMAC service function
         *  Inputs:
         *      0 : Header Code[0x00040106]
         *      1 : NCCH Program ID
         *      2 : bool is_out_box?
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : HMAC key address
         *      9 : Descriptor for mapping a write-only buffer in the target process
         *     10 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Read size
         *      3 : Descriptor for mapping a read-only buffer in the target process
         *      4 : Message ID address
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : HMAC key address
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         */
        void ReadMessageWithHMAC(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteRawFile service function
         *  Inputs:
         *      0 : Header Code[0x00050042]
         *      1 : Buffer size(unused)
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         */
        void WriteRawFile(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteMessage service function
         *  Inputs:
         *      0 : Header Code[0x00060104]
         *      1 : NCCH Program ID
         *      2 : bool is_out_box?
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Buffer address
         *      7 : Descriptor for mapping a read/write buffer in the target process
         *      8 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *      4 : Descriptor for mapping a read/write buffer in the target process
         *      5 : Message ID address
         */
        void WriteMessage(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteMessageWithHMAC service function
         *  Inputs:
         *      0 : Header Code[0x00070106]
         *      1 : NCCH Program ID
         *      2 : bool is_out_box?
         *      3 : Message ID size(unused, always 8)
         *      4 : Buffer size(unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Buffer address
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : HMAC key address
         *      9 : Descriptor for mapping a read/write buffer in the target process
         *     10 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : HMAC key address
         *      6 : Descriptor for mapping a read/write buffer in the target process
         *      7 : Message ID address
         */
        void WriteMessageWithHMAC(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Delete service function
         *  Inputs:
         *      0 : Header Code[0x00080102]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : bool is_out_box?
         *      4 : Message ID size (unused)
         *      5 : Descriptor for mapping a read-only buffer in the target process
         *      6 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Message ID address
         */
        void Delete(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetSystemInfo service function
         *  Inputs:
         *      0 : Header Code[0x000A00C4]
         *      1 : Destination buffer size (unused)
         *      2 : Info type
         *      3 : Param buffer size (unused)
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : Param buffer address
         *      6 : Descriptor for mapping a write-only buffer in the target process
         *      7 : Destination buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Param buffer address
         *      4 : Descriptor for mapping a write-only buffer in the target process
         *      5 : Destination buffer address
         */
        void GetSystemInfo(Kernel::HLERequestContext& ctx);

        /**
         * CECD::RunCommand service function
         *  Inputs:
         *      0 : Header Code[0x000B0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RunCommand(Kernel::HLERequestContext& ctx);

        /**
         * CECD::RunCommandAlt service function
         *  Inputs:
         *      0 : Header Code[0x000C0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void RunCommandAlt(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetCecInfoBuffer service function
         *  Inputs:
         *      0 : Header Code[0x000D0082]
         *      1 : unknown
         *      2 : unknown
         *      3 : buffer descriptor
         *      4 : buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 :
         */
        void GetCecInfoBuffer(Kernel::HLERequestContext& ctx);

        /**
         * GetCecStateAbbreviated service function
         *  Inputs:
         *      0: Header Code[0x000E0000]
         *  Outputs:
         *      1: ResultCode
         *      2: CecStateAbbreviated
         */
        void GetCecStateAbbreviated(Kernel::HLERequestContext& ctx);

        /**
         * GetCecInfoEventHandle service function
         *  Inputs:
         *      0: Header Code[0x000F0000]
         *  Outputs:
         *      1: ResultCode
         *      3: Event Handle
         */
        void GetCecInfoEventHandle(Kernel::HLERequestContext& ctx);

        /**
         * GetChangeStateEventHandle service function
         *  Inputs:
         *      0: Header Code[0x00100000]
         *  Outputs:
         *      1: ResultCode
         *      3: Event Handle
         */
        void GetChangeStateEventHandle(Kernel::HLERequestContext& ctx);

        /**
         * CECD::OpenAndWrite service function
         *  Inputs:
         *      0 : Header Code[0x00110104]
         *      1 : Buffer size (unused)
         *      2 : NCCH Program ID
         *      3 : Path type
         *      4 : File open flag?
         *      5 : Descriptor for process ID
         *      6 : Placeholder for process ID
         *      7 : Descriptor for mapping a read-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Buffer address
         */
        void OpenAndWrite(Kernel::HLERequestContext& ctx);

        /**
         * CECD::OpenAndRead service function
         *  Inputs:
         *      0 : Header Code[0x00120104]
         *      1 : Buffer size (unused)
         *      2 : NCCH Program ID
         *      3 : Path type
         *      4 : File open flag?
         *      5 : Descriptor for process ID
         *      6 : Placeholder for process ID
         *      7 : Descriptor for mapping a write-only buffer in the target process
         *      8 : Buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Toal bytes read
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Buffer address
         */
        void OpenAndRead(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetEventLog service function
         *  Inputs:
         *      0 : Header Code[0x001E0082]
         *      1 : unknown
         *      2 : unknown
         *      3 : buffer descriptor
         *      4 : buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : unknown
         */
        void GetEventLog(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetEventLogStart service function
         *  Inputs:
         *      0 : Header Code[0x001F0000]
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : unknown
         */
        void GetEventLogStart(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> cecd;
    };

private:
    std::string GetCecDataPathTypeAsString(const CecDataPathType type, const u32 program_id,
                                           const std::vector<u8>& message_id = std::vector<u8>());

    const std::vector<u8> cecd_system_savedata_id = {
        0x00, 0x00, 0x00, 0x00, 0x26, 0x00, 0x01, 0x00
    };

    Service::FS::ArchiveHandle cecd_system_save_data_archive;

    Kernel::SharedPtr<Kernel::Event> cecinfo_event;
    Kernel::SharedPtr<Kernel::Event> change_state_event;
};

/// Initialize CECD service(s)
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace CECD
} // namespace Service
