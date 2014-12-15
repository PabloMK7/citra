// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/am_app.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace AM_APP

namespace AM_APP {

const Interface::FunctionInfo FunctionTable[] = {
};
////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
