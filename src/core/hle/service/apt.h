// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Service

namespace Service {

// Application and title launching service. These services handle signaling for home/power button as
// well. Only one session for either APT service can be open at a time, normally processes close the
// service handle immediately once finished using the service. The commands for APT:U and APT:S are 
// exactly the same, however certain commands are only accessible with APT:S(NS module will call 
// svcBreak when the command isn't accessible). See http://3dbrew.org/wiki/NS#APT_Services.

class APT_U : public Interface {
public:

    APT_U() {
    }

    ~APT_U() {
    }

    enum {
        CMD_OFFSET                              = 0x00000080,

        CMD_HEADER_INIT                         = 0x00020080,   ///< Initialize service
        CMD_HEADER_GET_LOCK_HANDLE              = 0x00010040,   ///< Get service Mutex
        CMD_HEADER_ENABLE                       = 0x00030040,   ///< Enable service
        CMD_HEADER_INQUIRE_NOTIFICATION         = 0x000B0040,   ///< Inquire notification
        CMD_HEADER_PREPARE_TO_JUMP_TO_HOME_MENU = 0x002B0000,   ///< Prepare to jump to home menu
        CMD_HEADER_JUMP_TO_HOME_MENU            = 0x002C0044,   ///< Jump to home menu
        CMD_HEADER_NOTIFY_TO_WAIT               = 0x00430040,   ///< Notify to wait
        CMD_HEADER_APPLET_UTILITY               = 0x004B00C2,   ///< Applet utility
        CMD_HEADER_GLANCE_PARAMETER             = 0x000E0080,   ///< Glance parameter
        CMD_HEADER_RECEIVE_PARAMETER            = 0x000D0080,   ///< Receive parameter
        CMD_HEADER_REPLY_SLEEP_QUERY            = 0x003E0080,   ///< Reply sleep query
        CMD_HEADER_PREPARE_TO_CLOSE_APP         = 0x00220040,   ///< Prepare to close application
        CMD_HEADER_CLOSE_APP                    = 0x00270044,   ///< Close application
    };

    /**
     * Gets the string port name used by CTROS for the APT service
     * @return Port name of service
     */
    std::string GetPortName() const {
        return "APT:U";
    }

    /**
     * Called when svcSendSyncRequest is called, loads command buffer and executes comand
     * @return Return result of svcSendSyncRequest passed back to user app
     */
    Syscall::Result Sync();

private:

    Syscall::Result GetLockHandle();

    DISALLOW_COPY_AND_ASSIGN(APT_U);
};

} // namespace
