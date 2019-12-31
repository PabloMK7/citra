// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::MVD {

class MVD_STD final : public ServiceFramework<MVD_STD> {
public:
    MVD_STD();
    ~MVD_STD() = default;

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::SessionRequestHandler>(*this);
    }
    friend class boost::serialization::access;
};

} // namespace Service::MVD

BOOST_CLASS_EXPORT_KEY(Service::MVD::MVD_STD)
