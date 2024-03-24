// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/news/news.h"

namespace Service::NEWS {

class NEWS_U final : public Module::Interface {
public:
    explicit NEWS_U(std::shared_ptr<Module> news);
};

} // namespace Service::NEWS
