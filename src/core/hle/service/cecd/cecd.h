// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

class Module final {
public:
    Module();
    ~Module();

    enum class CecCommand : u32 {
        NONE = 0,
        START = 1,
        RESET_START = 2,
        READYSCAN = 3,
        READYSCANWAIT = 4,
        STARTSCAN = 5,
        RESCAN = 6,
        NDM_RESUME = 7,
        NDM_SUSPEND = 8,
        NDM_SUSPEND_IMMEDIATE = 9,
        STOPWAIT = 0x0A,
        STOP = 0x0B,
        STOP_FORCE = 0x0C,
        STOP_FORCE_WAIT = 0x0D,
        RESET_FILTER = 0x0E,
        DAEMON_STOP = 0x0F,
        DAEMON_START = 0x10,
        EXIT = 0x11,
        OVER_BOSS = 0x12,
        OVER_BOSS_FORCE = 0x13,
        OVER_BOSS_FORCE_WAIT = 0x14,
        END = 0x15,
    };

    /**
     * CecDataPathType possible missing values; need to figure out placement
     *
     * data:/CEC/TMP
     * data:/CEC/test
     */
    enum class CecDataPathType : u32 {
        INVALID = 0,
        MBOX_LIST = 1,         /// data:/CEC/MBoxList____
        MBOX_INFO = 2,         /// data:/CEC/<id>/MBoxInfo____
        INBOX_INFO = 3,        /// data:/CEC/<id>/InBox___/BoxInfo_____
        OUTBOX_INFO = 4,       /// data:/CEC/<id>/OutBox__/BoxInfo_____
        OUTBOX_INDEX = 5,      /// data:/CEC/<id>/OutBox__/OBIndex_____
        INBOX_MSG = 6,         /// data:/CEC/<id>/InBox___/_<message_id>
        OUTBOX_MSG = 7,        /// data:/CEC/<id>/OutBox__/_<message_id>
        ROOT_DIR = 10,         /// data:/CEC
        MBOX_DIR = 11,         /// data:/CEC/<id>
        INBOX_DIR = 12,        /// data:/CEC/<id>/InBox___
        OUTBOX_DIR = 13,       /// data:/CEC/<id>/OutBox__
        MBOX_DATA = 100,       /// data:/CEC/<id>/MBoxData.0<i-100>
        MBOX_ICON = 101,       /// data:/CEC/<id>/MBoxData.001
        MBOX_TITLE = 110,      /// data:/CEC/<id>/MBoxData.010
        MBOX_PROGRAM_ID = 150, /// data:/CEC/<id>/MBoxData.050
    };

    enum class CecState : u32 {
        NONE = 0,
        INIT = 1,
        WIRELESS_PARAM_SETUP = 2,
        WIRELESS_READY = 3,
        WIRELESS_START_CONFIG = 4,
        SCAN = 5,
        SCANNING = 6,
        CONNECT = 7,
        CONNECTING = 8,
        CONNECTED = 9,
        CONNECT_TCP = 10,
        CONNECTING_TCP = 11,
        CONNECTED_TCP = 12,
        NEGOTIATION = 13,
        SEND_RECV_START = 14,
        SEND_RECV_INIT = 15,
        SEND_READY = 16,
        RECEIVE_READY = 17,
        RECEIVE = 18,
        CONNECTION_FINISH_TCP = 19,
        CONNECTION_FINISH = 20,
        SEND_POST = 21,
        RECEIVE_POST = 22,
        FINISHING = 23,
        FINISH = 24,
        OVER_BOSS = 25,
        IDLE = 26
    };

    enum class CecSystemInfoType : u32 { EulaVersion = 1, Eula = 2, ParentControl = 3 };

    struct CecInOutBoxInfoHeader {
        u16_le magic; // 0x6262 'bb'
        INSERT_PADDING_BYTES(2);
        u32_le box_info_size;
        u32_le max_box_size;
        u32_le box_size;
        u32_le max_message_num;
        u32_le message_num;
        u32_le max_batch_size;
        u32_le max_message_size;
    };
    static_assert(sizeof(CecInOutBoxInfoHeader) == 0x20,
                  "CecInOutBoxInfoHeader struct has incorrect size.");

    struct CecMBoxInfoHeader {
        u16_le magic; // 0x6363 'cc'
        INSERT_PADDING_BYTES(2);
        u32_le program_id;
        u32_le private_id;
        u8 flag;
        u8 flag2;
        INSERT_PADDING_BYTES(2);
        std::array<u8, 32> hmac_key;
        INSERT_PADDING_BYTES(4);
        struct Time {
            u32_le year;
            u8 month;
            u8 day;
            u8 hour;
            u8 minute;
            u8 second;
            u8 millisecond;
            u8 microsecond;
            u8 padding;
        } last_accessed;
        INSERT_PADDING_BYTES(4);
        Time last_received;
        INSERT_PADDING_BYTES(4);
        Time unknown_time;
    };
    static_assert(sizeof(CecMBoxInfoHeader) == 0x60,
                  "CecMBoxInfoHeader struct has incorrect size.");

    struct CecMBoxListHeader {
        u16_le magic; // 0x6868 'hh'
        INSERT_PADDING_BYTES(2);
        u16_le version; // 0x01 00, maybe activated flag?
        INSERT_PADDING_BYTES(2);
        u16_le num_boxes; // 24 max
        INSERT_PADDING_BYTES(2);
        std::array<std::array<u8, 16>, 24> box_names; // 16 char names, 24 boxes
    };
    static_assert(sizeof(CecMBoxListHeader) == 0x18C,
                  "CecMBoxListHeader struct has incorrect size.");

    struct CecMessageHeader {
        u16_le magic; // 0x6060 ``
        INSERT_PADDING_BYTES(2);
        u32_le message_size;
        u32_le header_size;
        u32_le body_size;

        u32_le title_id;
        u32_le title_id_2;
        u32_le batch_id;
        u32_le unknown_id;

        std::array<u8, 8> message_id;
        u32_le version;
        std::array<u8, 8> message_id_2;
        u8 flag;
        u8 send_method;
        u8 is_unopen;
        u8 is_new;
        u64_le sender_id;
        u64_le sender_id2;
        struct Time {
            u32_le year;
            u8 month;
            u8 day;
            u8 hour;
            u8 minute;
            u8 second;
            u8 millisecond;
            u8 microsecond;
            u8 padding;
        } send_time, recv_time, create_time;
        u8 send_count;
        u8 foward_count;
        u16_le user_data;
    };
    static_assert(sizeof(CecMessageHeader) == 0x70, "CecMessageHeader struct has incorrect size.");

    struct CecOBIndexHeader {
        u16_le magic; // 0x6767 'gg'
        INSERT_PADDING_BYTES(2);
        u32_le message_num;
        // Array of messageid's 8 bytes each, same as CecMessageHeader.message_id[8]
    };
    static_assert(sizeof(CecOBIndexHeader) == 0x08, "CecOBIndexHeader struct has incorrect size.");

    enum class CecdState : u32 {
        NDM_STATUS_WORKING = 0,
        NDM_STATUS_IDLE = 1,
        NDM_STATUS_SUSPENDING = 2,
        NDM_STATUS_SUSPENDED = 3,
    };

    union CecOpenMode {
        u32 raw;
        BitField<0, 1, u32> unknown; // 1 delete?
        BitField<1, 1, u32> read;    // 2
        BitField<2, 1, u32> write;   // 4
        BitField<3, 1, u32> create;  // 8
        BitField<4, 1, u32> check;   // 16 maybe validate sig?
        BitField<30, 1, u32> unk_flag;
    };

    enum class CecTest : u32 {
        CEC_TEST_000 = 0,
        CEC_TEST_001 = 1,
        CEC_TEST_002 = 2,
        CEC_TEST_003 = 3,
        CEC_TEST_004 = 4,
        CEC_TEST_005 = 5,
        CEC_TEST_006 = 6,
    };

    /// Opening a file and reading/writing can be handled by two different functions
    /// So, we need to pass that file data around
    struct SessionData : public Kernel::SessionRequestHandler::SessionDataBase {
        SessionData();
        ~SessionData();

        u32 ncch_program_id;
        CecDataPathType data_path_type;
        CecOpenMode open_mode;
        FileSys::Path path;

        std::shared_ptr<Service::FS::File> file;
    };

    class Interface : public ServiceFramework<Interface, SessionData> {
    public:
        Interface(std::shared_ptr<Module> cecd, const char* name, u32 max_session);
        ~Interface() = default;

    protected:
        /**
         * CECD::Open service function
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
        void Open(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Read service function
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
        void Read(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadMessage service function
         *  Inputs:
         *      0 : Header Code[0x00030104]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
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
         *      2 : bool is_outbox
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
         * CECD::Write service function
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
        void Write(Kernel::HLERequestContext& ctx);

        /**
         * CECD::WriteMessage service function
         *  Inputs:
         *      0 : Header Code[0x00060104]
         *      1 : NCCH Program ID
         *      2 : bool is_outbox
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
         *      2 : bool is_outbox
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
         *      3 : bool is_outbox
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
         * CECD::SetData service function
         *  Inputs:
         *      0 : Header Code[0x000900C2]
         *      1 : NCCH Program ID
         *      2 : Path type
         *      3 : bool is_outbox
         *      4 : Descriptor for mapping a read-only buffer in the target process
         *      5 : Message ID address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *      2 : Descriptor for mapping a read-only buffer in the target process
         *      3 : Message ID address
         */
        void SetData(Kernel::HLERequestContext& ctx);

        /**
         * CECD::ReadData service function
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
        void ReadData(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Start service function
         *  Inputs:
         *      0 : Header Code[0x000B0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Start(Kernel::HLERequestContext& ctx);

        /**
         * CECD::Stop service function
         *  Inputs:
         *      0 : Header Code[0x000C0040]
         *      1 : Command
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         */
        void Stop(Kernel::HLERequestContext& ctx);

        /**
         * CECD::GetCecInfoBuffer service function
         *  Inputs:
         *      0 : Header Code[0x000D0082]
         *      1 : unknown
         *      2 : unknown, buffer size?
         *      3 : Descriptor for mapping a write-only buffer in the target process
         *      4 : Destination buffer address
         *  Outputs:
         *      1 : Result of function, 0 on success, otherwise error code
         *    2-3 : MappedBuffer
         */
        void GetCecInfoBuffer(Kernel::HLERequestContext& ctx);

        /**
         * GetCecdState service function
         *  Inputs:
         *      0: Header Code[0x000E0000]
         *  Outputs:
         *      1: ResultCode
         *      2: CecdState
         */
        void GetCecdState(Kernel::HLERequestContext& ctx);

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
         *      4 : File open flag
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
         *      4 : File open flag
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

        /**
         * GetCecInfoEventHandleSys service function
         *  Inputs:
         *      0: Header Code[0x40020002]
         *  Outputs:
         *      1: ResultCode
         *      3: Event Handle
         */
        void GetCecInfoEventHandleSys(Kernel::HLERequestContext& ctx);

    private:
        std::shared_ptr<Module> cecd;
    };

private:
    /// String used by cecd for base64 encoding found in the sysmodule disassembly
    const std::string base64_dict =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

    const std::vector<u8> cecd_system_savedata_id = {0x00, 0x00, 0x00, 0x00,
                                                     0x26, 0x00, 0x01, 0x00};

    /// Encoding function used for the message id
    std::string EncodeBase64(const std::vector<u8>& in, const std::string& dictionary) const;

    std::string GetCecDataPathTypeAsString(const CecDataPathType type, const u32 program_id,
                                           const std::vector<u8>& msg_id = std::vector<u8>()) const;

    std::string GetCecCommandAsString(const CecCommand command) const;

    void CheckAndUpdateFile(const CecDataPathType path_type, const u32 ncch_program_id,
                            std::vector<u8>& file_buffer);

    Service::FS::ArchiveHandle cecd_system_save_data_archive;

    Kernel::SharedPtr<Kernel::Event> cecinfo_event;
    Kernel::SharedPtr<Kernel::Event> change_state_event;
};

/// Initialize CECD service(s)
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace CECD
} // namespace Service
