// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <QImage>
#include "citra_qt/camera/camera_util.h"
#include "citra_qt/camera/qt_camera_factory.h"
#include "core/frontend/camera/interface.h"

namespace Camera {

class StillImageCamera final : public CameraInterface {
public:
    StillImageCamera(QImage image);
    void StartCapture() override;
    void StopCapture() override;
    void SetResolution(const Service::CAM::Resolution&) override;
    void SetFlip(Service::CAM::Flip) override;
    void SetEffect(Service::CAM::Effect) override;
    void SetFormat(Service::CAM::OutputFormat) override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override {}
    std::vector<u16> ReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    QImage image;
    int width, height;
    bool output_rgb;
    bool flip_horizontal, flip_vertical;
};

class StillImageCameraFactory final : public QtCameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& config) const override;

private:
    static const std::string getFilePath();
};

} // namespace Camera
