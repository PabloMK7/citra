// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/qtm/qtm_s.h"

namespace Service {
namespace QTM {

const Interface::FunctionInfo FunctionTable[] = {
    // clang-format off
    // qtm common commands
    {0x00010080, nullptr, "GetHeadtrackingInfoRaw"},
    {0x00020080, nullptr, "GetHeadtrackingInfo"},
    // clang-format on
};

QTM_S::QTM_S() {
    Register(FunctionTable);
}

} // namespace QTM
} // namespace Service
