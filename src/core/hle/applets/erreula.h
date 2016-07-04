// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/applets/applet.h"
#include "core/hle/kernel/shared_memory.h"

namespace HLE {
namespace Applets {

class ErrEula final : public Applet {
public:
    explicit ErrEula(Service::APT::AppletId id): Applet(id) { }

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;
    bool IsRunning() const override { return started; }

    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the application with GSPGPU::ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;
private:
    /// Whether this applet is currently running instead of the host application or not.
    bool started = false;
};

} // namespace Applets
} // namespace HLE
