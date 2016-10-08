// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/boss/boss.h"
#include "core/hle/service/boss/boss_p.h"
#include "core/hle/service/boss/boss_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace BOSS {

static u32 new_arrival_flag;
static u32 ns_data_new_flag;
static u32 output_flag;

void InitializeSession(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u64 unk_param = ((u64)cmd_buff[1] | ((u64)cmd_buff[2] << 32));
    u32 translation = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];

    if (translation != IPC::CallingPidDesc()) {
        cmd_buff[0] = IPC::MakeHeader(0, 0x1, 0); // 0x40
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent)
                          .raw;
        LOG_ERROR(Service_BOSS, "The translation was invalid, translation=0x%08X", translation);
        return;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param=0x%016X, translation=0x%08X, unk_param4=0x%08X",
                unk_param, translation, unk_param4);
}

void RegisterStorage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_flag = cmd_buff[4] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x2, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(
        Service_BOSS,
        "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, unk_flag=0x%08X",
        unk_param1, unk_param2, unk_param3, unk_flag);
}

void UnregisterStorage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x3, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void GetStorageInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x4, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void RegisterPrivateRootCa(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x5, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                translation, buff_addr, buff_size);
}

void RegisterPrivateClientCert(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 translation1 = cmd_buff[3];
    u32 buff1_addr = cmd_buff[4];
    u32 buff1_size = (translation1 >> 4);
    u32 translation2 = cmd_buff[5];
    u32 buff2_addr = cmd_buff[6];
    u32 buff2_size = (translation2 >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x6, 0x1, 0x4);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff1_size << 4 | 0xA);
    cmd_buff[3] = buff1_addr;
    cmd_buff[2] = (buff2_size << 4 | 0xA);
    cmd_buff[3] = buff2_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, "
                              "translation1=0x%08X, buff1_addr=0x%08X, buff1_size=0x%08X, "
                              "translation2=0x%08X, buff2_addr=0x%08X, buff2_size=0x%08X",
                unk_param1, unk_param2, translation1, buff1_addr, buff1_size, translation2,
                buff2_addr, buff2_size);
}

void GetNewArrivalFlag(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x7, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = new_arrival_flag;

    LOG_WARNING(Service_BOSS, "(STUBBED) new_arrival_flag=%u", new_arrival_flag);
}

void RegisterNewArrivalEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];

    cmd_buff[0] = IPC::MakeHeader(0x8, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X", unk_param1,
                unk_param2);
}

void SetOptoutFlag(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    output_flag = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x9, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "output_flag=%u", output_flag);
}

void GetOptoutFlag(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xA, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = output_flag;

    LOG_WARNING(Service_BOSS, "output_flag=%u", output_flag);
}

void RegisterTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 unk_param3 = cmd_buff[3] & 0xFF;
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0xB, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, translation, buff_addr, buff_size);
}

void UnregisterTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0xC, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void ReconfigureTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0xD, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void GetTaskIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xE, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void GetStepIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0xF, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                translation, buff_addr, buff_size);
}

void GetNsDataIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 translation = cmd_buff[5];
    u32 buff_addr = cmd_buff[6];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x10, 0x3, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (16 bit value)
    cmd_buff[3] = 0; // stub 0 (16 bit value)
    cmd_buff[4] = (buff_size << 4 | 0xC);
    cmd_buff[5] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, translation, buff_addr, buff_size);
}

void GetOwnNsDataIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 translation = cmd_buff[5];
    u32 buff_addr = cmd_buff[6];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x11, 0x3, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (16 bit value)
    cmd_buff[3] = 0; // stub 0 (16 bit value)
    cmd_buff[4] = (buff_size << 4 | 0xC);
    cmd_buff[5] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, translation, buff_addr, buff_size);
}

void GetNewDataNsDataIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 translation = cmd_buff[5];
    u32 buff_addr = cmd_buff[6];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x12, 0x3, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (16 bit value)
    cmd_buff[3] = 0; // stub 0 (16 bit value)
    cmd_buff[4] = (buff_size << 4 | 0xC);
    cmd_buff[5] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, translation, buff_addr, buff_size);
}

void GetOwnNewDataNsDataIdList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 translation = cmd_buff[5];
    u32 buff_addr = cmd_buff[6];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x13, 0x3, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (16 bit value)
    cmd_buff[3] = 0; // stub 0 (16 bit value)
    cmd_buff[4] = (buff_size << 4 | 0xC);
    cmd_buff[5] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, translation, buff_addr, buff_size);
}

void SendProperty(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x14, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void SendPropertyHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x15, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void ReceiveProperty(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 buff_size = cmd_buff[2];
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x16, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32 bit value)
    cmd_buff[2] = (buff_size << 4 | 0xC);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, buff_size=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X",
                unk_param1, buff_size, translation, buff_addr);
}

void UpdateTaskInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x17, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void UpdateTaskCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 buff_size = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x18, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) buff_size=0x%08X, unk_param2=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X",
                buff_size, unk_param2, translation, buff_addr);
}

void GetTaskInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x19, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 ( 32bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x1A, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 ( 32bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskServiceStatus(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x1B, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 ( 8bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void StartTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x1C, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void StartTaskImmediate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x1D, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void CancelTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x1E, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskFinishHandle(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1F, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    cmd_buff[3] = 0; // stub 0(This should be a handle of task_finish ?)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void GetTaskState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 buff_size = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[3];
    u32 buff_addr = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x20, 0x4, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (8 bit value)
    cmd_buff[3] = 0; // stub 0 (32 bit value)
    cmd_buff[4] = 0; // stub 0 (8 bit value)
    cmd_buff[5] = (buff_size << 4 | 0xA);
    cmd_buff[6] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) buff_size=0x%08X, unk_param2=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X",
                buff_size, unk_param2, translation, buff_addr);
}

void GetTaskResult(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x21, 0x4, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (8 bit value)
    cmd_buff[3] = 0; // stub 0 (32 bit value)
    cmd_buff[4] = 0; // stub 0 (8 bit value)
    cmd_buff[5] = (buff_size << 4 | 0xA);
    cmd_buff[6] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskCommErrorCode(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x22, 0x4, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32 bit value)
    cmd_buff[3] = 0; // stub 0 (32 bit value)
    cmd_buff[4] = 0; // stub 0 (8 bit value)
    cmd_buff[5] = (buff_size << 4 | 0xA);
    cmd_buff[6] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskStatus(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 unk_param3 = cmd_buff[3] & 0xFF;
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x23, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (8 bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, translation, buff_addr, buff_size);
}

void GetTaskError(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x24, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (8 bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void GetTaskInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x25, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, translation, buff_addr, buff_size);
}

void DeleteNsData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x26, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X", unk_param1);
}

void GetNsDataHeaderInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 unk_param3 = cmd_buff[3];
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x27, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xC);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, translation, buff_addr, buff_size);
}

void ReadNsData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 translation = cmd_buff[5];
    u32 buff_addr = cmd_buff[6];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x28, 0x3, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)
    cmd_buff[3] = 0; // stub 0 (32bit value)
    cmd_buff[4] = (buff_size << 4 | 0xC);
    cmd_buff[5] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, translation=0x%08X, "
                              "buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, translation, buff_addr, buff_size);
}

void SetNsDataAdditionalInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];

    cmd_buff[0] = IPC::MakeHeader(0x29, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X", unk_param1,
                unk_param2);
}

void GetNsDataAdditionalInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x2A, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X", unk_param1);
}

void SetNsDataNewFlag(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    ns_data_new_flag = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x2B, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, ns_data_new_flag=0x%08X", unk_param1,
                ns_data_new_flag);
}

void GetNsDataNewFlag(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x2C, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = ns_data_new_flag;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, ns_data_new_flag=0x%08X", unk_param1,
                ns_data_new_flag);
}

void GetNsDataLastUpdate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x2D, 0x3, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)
    cmd_buff[3] = 0; // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X", unk_param1);
}

void GetErrorCode(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x2E, 0x2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X", unk_param1);
}

void RegisterStorageEntry(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];
    u32 unk_param5 = cmd_buff[5] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x2F, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED)  unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "unk_param4=0x%08X, unk_param5=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4, unk_param5);
}

void GetStorageEntryInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x30, 0x3, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)
    cmd_buff[3] = 0; // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void SetStorageOption(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1] & 0xFF;
    u32 unk_param2 = cmd_buff[2];
    u32 unk_param3 = cmd_buff[3];
    u32 unk_param4 = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x31, 0x1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_BOSS, "(STUBBED)  unk_param1=0x%08X, unk_param2=0x%08X, "
                              "unk_param3=0x%08X, unk_param4=0x%08X",
                unk_param1, unk_param2, unk_param3, unk_param4);
}

void GetStorageOption(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x32, 0x5, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (32bit value)
    cmd_buff[3] = 0; // stub 0 (8bit value)
    cmd_buff[4] = 0; // stub 0 (16bit value)
    cmd_buff[5] = 0; // stub 0 (16bit value)

    LOG_WARNING(Service_BOSS, "(STUBBED) called");
}

void StartBgImmediate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x33, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff_size << 4 | 0xA);
    cmd_buff[3] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) buff_size=0x%08X, unk_param2=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void GetTaskActivePriority(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 unk_param1 = cmd_buff[1]; // TODO(JamePeng): Figure out the meaning of these parameters
    u32 translation = cmd_buff[2];
    u32 buff_addr = cmd_buff[3];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x34, 0x2, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // stub 0 (8bit value)
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) buff_size=0x%08X, unk_param2=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X",
                unk_param1, translation, buff_addr, buff_size);
}

void RegisterImmediateTask(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2] & 0xFF;
    u32 unk_param3 = cmd_buff[3] & 0xFF;
    u32 translation = cmd_buff[4];
    u32 buff_addr = cmd_buff[5];
    u32 buff_size = (translation >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x35, 0x1, 0x2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = (buff_size << 4 | 0xA);
    cmd_buff[4] = buff_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, unk_param3=0x%08X, "
                              "translation=0x%08X, buff_addr=0x%08X, buff_size=0x%08X",
                unk_param1, unk_param2, unk_param3, translation, buff_addr, buff_size);
}

void SetTaskQuery(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 translation1 = cmd_buff[3];
    u32 buff1_addr = cmd_buff[4];
    u32 buff1_size = (translation1 >> 4);
    u32 translation2 = cmd_buff[5];
    u32 buff2_addr = cmd_buff[6];
    u32 buff2_size = (translation2 >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x36, 0x1, 0x4);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff1_size << 4 | 0xA);
    cmd_buff[3] = buff1_addr;
    cmd_buff[2] = (buff2_size << 4 | 0xA);
    cmd_buff[3] = buff2_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, "
                              "translation1=0x%08X, buff1_addr=0x%08X, buff1_size=0x%08X, "
                              "translation2=0x%08X, buff2_addr=0x%08X, buff2_size=0x%08X",
                unk_param1, unk_param2, translation1, buff1_addr, buff1_size, translation2,
                buff2_addr, buff2_size);
}

void GetTaskQuery(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    // TODO(JamePeng): Figure out the meaning of these parameters
    u32 unk_param1 = cmd_buff[1];
    u32 unk_param2 = cmd_buff[2];
    u32 translation1 = cmd_buff[3];
    u32 buff1_addr = cmd_buff[4];
    u32 buff1_size = (translation1 >> 4);
    u32 translation2 = cmd_buff[5];
    u32 buff2_addr = cmd_buff[6];
    u32 buff2_size = (translation2 >> 4);

    cmd_buff[0] = IPC::MakeHeader(0x37, 0x1, 0x4);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = (buff1_size << 4 | 0xA);
    cmd_buff[3] = buff1_addr;
    cmd_buff[2] = (buff2_size << 4 | 0xC);
    cmd_buff[3] = buff2_addr;

    LOG_WARNING(Service_BOSS, "(STUBBED) unk_param1=0x%08X, unk_param2=0x%08X, "
                              "translation1=0x%08X, buff1_addr=0x%08X, buff1_size=0x%08X, "
                              "translation2=0x%08X, buff2_addr=0x%08X, buff2_size=0x%08X",
                unk_param1, unk_param2, translation1, buff1_addr, buff1_size, translation2,
                buff2_addr, buff2_size);
}

void Init() {
    using namespace Kernel;

    AddService(new BOSS_P_Interface);
    AddService(new BOSS_U_Interface);

    new_arrival_flag = 0;
    ns_data_new_flag = 0;
    output_flag = 0;
}

void Shutdown() {}

} // namespace BOSS

} // namespace Service
