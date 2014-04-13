// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "common/log.h"
#include "core/hle/service/apt.h"




namespace Service {


Syscall::Result APT::Sync() {
    NOTICE_LOG(HLE, "APT::Sync - Initialize");
    return 0;
}


}
