// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"

#include "core/hle/applets/applet.h"
#include "core/hle/applets/swkbd.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace HLE {
namespace Applets {

static std::unordered_map<Service::APT::AppletId, std::shared_ptr<Applet>> applets;

ResultCode Applet::Create(Service::APT::AppletId id) {
    switch (id) {
    case Service::APT::AppletId::SoftwareKeyboard1:
    case Service::APT::AppletId::SoftwareKeyboard2:
        applets[id] = std::make_shared<SoftwareKeyboard>(id);
        break;
    default:
        // TODO(Subv): Find the right error code
        return ResultCode(ErrorDescription::NotFound, ErrorModule::Applet, ErrorSummary::NotSupported, ErrorLevel::Permanent);
    }

    return RESULT_SUCCESS;
}

std::shared_ptr<Applet> Applet::Get(Service::APT::AppletId id) {
    auto itr = applets.find(id);
    if (itr != applets.end())
        return itr->second;
    return nullptr;
}

}
} // namespace
