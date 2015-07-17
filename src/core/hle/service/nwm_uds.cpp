// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/nwm_uds.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NWM_UDS

namespace NWM_UDS {

static Kernel::SharedPtr<Kernel::Event> handle_event;

/**
 * NWM_UDS::Shutdown service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Shutdown(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_NWM, "(STUBBED) called");
}

/**
 * NWM_UDS::RecvBeaconBroadcastData service function
 *  Inputs:
 *      1 : Output buffer max size
 *      2 : Unknown
 *      3 : Unknown
 *      4 : MAC address?
 *   6-14 : Unknown, usually zero / uninitialized?
 *     15 : WLan Comm ID
 *     16 : This is the ID also located at offset 0xE in the CTR-generation structure.
 *     17 : Value 0
 *     18 : Input handle
 *     19 : (Size<<4) | 12
 *     20 : Output buffer ptr
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RecvBeaconBroadcastData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 out_buffer_size = cmd_buff[1];
    u32 unk1            = cmd_buff[2];
    u32 unk2            = cmd_buff[3];
    u32 mac_address     = cmd_buff[4];

    u32 unk3            = cmd_buff[6];

    u32 wlan_comm_id    = cmd_buff[15];
    u32 ctr_gen_id      = cmd_buff[16];
    u32 value           = cmd_buff[17];
    u32 input_handle    = cmd_buff[18];
    u32 new_buffer_size = cmd_buff[19];
    u32 out_buffer_ptr  = cmd_buff[20];

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_NWM, "(STUBBED) called out_buffer_size=0x%08X, unk1=0x%08X, unk2=0x%08X,"
                "mac_address=0x%08X, unk3=0x%08X, wlan_comm_id=0x%08X, ctr_gen_id=0x%08X,"
                "value=%u, input_handle=0x%08X, new_buffer_size=0x%08X, out_buffer_ptr=0x%08X",
                out_buffer_size, unk1, unk2, mac_address, unk3, wlan_comm_id, ctr_gen_id, value,
                input_handle, new_buffer_size, out_buffer_ptr);
}

/**
 * NWM_UDS::Initialize service function
 *  Inputs:
 *      1 : Unknown
 *   2-11 : Input Structure
 *     12 : Unknown u16
 *     13 : Value 0
 *     14 : Handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Value 0
 *      3 : Output handle
 */
static void Initialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 unk1   = cmd_buff[1];
    u32 unk2   = cmd_buff[12];
    u32 value  = cmd_buff[13];
    u32 handle = cmd_buff[14];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    cmd_buff[3] = Kernel::g_handle_table.Create(handle_event).MoveFrom(); //TODO(purpasmart): Verify if this is a event handle

    LOG_WARNING(Service_NWM, "(STUBBED) called unk1=0x%08X, unk2=0x%08X, value=%u, handle=0x%08X",
                unk1, unk2, value, handle);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00030000, Shutdown,                  "Shutdown"},
    {0x000F0404, RecvBeaconBroadcastData,   "RecvBeaconBroadcastData"},
    {0x00100042, nullptr,                   "SetBeaconAdditionalData"},
    {0x001400C0, nullptr,                   "RecvBroadcastDataFrame"},
    {0x001B0302, Initialize,                "Initialize"},
    {0x001D0044, nullptr,                   "BeginHostingNetwork"},
    {0x001E0084, nullptr,                   "ConnectToNetwork"},
    {0x001F0006, nullptr,                   "DecryptBeaconData"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    handle_event = Kernel::Event::Create(RESETTYPE_ONESHOT, "NWM_UDS::handle_event");

    Register(FunctionTable);
}

Interface::~Interface() {
    handle_event = nullptr;
}

} // namespace
