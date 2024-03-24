// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::QTM {

class QTM_C final : public ServiceFramework<QTM_C> {
public:
    QTM_C();
    ~QTM_C() = default;
};

} // namespace Service::QTM
