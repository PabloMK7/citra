// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <QImage>
#include "citra_qt/camera/camera_util.h"
#include "citra_qt/camera/qt_camera_base.h"
#include "core/frontend/camera/interface.h"

namespace Camera {

class StillImageCamera final : public QtCameraInterface {
public:
    StillImageCamera(QImage image, const Service::CAM::Flip& flip);
    void StartCapture() override;
    void StopCapture() override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override {}
    QImage QtReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    QImage image;
};

class StillImageCameraFactory final : public QtCameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) const override;

private:
    static const std::string GetFilePath();
};

} // namespace Camera
