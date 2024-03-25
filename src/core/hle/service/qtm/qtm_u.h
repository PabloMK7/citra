// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::QTM {

class QTM_U final : public ServiceFramework<QTM_U> {
public:
    QTM_U();
    ~QTM_U() = default;
};

} // namespace Service::QTM
