// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace Service::NEWS {

class NEWS_U final : public ServiceFramework<NEWS_U> {
public:
    NEWS_U();

private:
    SERVICE_SERIALIZATION_SIMPLE
};

} // namespace Service::NEWS

BOOST_CLASS_EXPORT_KEY(Service::NEWS::NEWS_U)
