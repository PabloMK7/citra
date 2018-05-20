// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "core/frontend/camera/factory.h"

namespace Camera {

// Base class for camera factories of citra_qt
class QtCameraFactory : public CameraFactory {
    std::unique_ptr<CameraInterface> CreatePreview(const std::string& config, int width, int height,
                                                   const Service::CAM::Flip& flip) const override;
};

} // namespace Camera
