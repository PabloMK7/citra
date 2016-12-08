// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service {
namespace NFC {

void Init() {
    AddService(new NFC_M());
    AddService(new NFC_U());
}

} // namespace NFC
} // namespace Service
