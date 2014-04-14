// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/apt.h"

namespace Service {

// Returns handle to APT Mutex. Not imlemented.
Syscall::Result APT::GetLockHandle() {
    return 0x00000000;
}

/**
 * Called when svcSendSyncRequest is called, loads command buffer and executes comand
 * @return Return result of svcSendSyncRequest passed back to user app
 */
Syscall::Result APT::Sync() {
    Syscall::Result res = 0;
    u32* cmd_buff = (u32*)HLE::GetPointer(HLE::CMD_BUFFER_ADDR + CMD_OFFSET);

    switch(cmd_buff[0]) {
    case CMD_HEADER_INIT:
        NOTICE_LOG(OSHLE, "APT::Sync - Initialize");
        break;

    case CMD_HEADER_GET_LOCK_HANDLE:
        NOTICE_LOG(OSHLE, "APT::Sync - GetLockHandle");
        cmd_buff[5] = GetLockHandle();
        break;

    default:
        ERROR_LOG(OSHLE, "APT::Sync - Unknown command 0x%08X", cmd_buff[0]);
        res = -1;
        break;
    }

    return res;
}


}
