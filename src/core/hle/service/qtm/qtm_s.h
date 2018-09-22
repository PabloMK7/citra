// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::QTM {

class QTM_S final : public ServiceFramework<QTM_S> {
public:
    QTM_S();
    ~QTM_S() = default;
};

} // namespace Service::QTM
