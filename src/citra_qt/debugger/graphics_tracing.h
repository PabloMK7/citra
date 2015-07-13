// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "graphics_breakpoint_observer.h"

class EmuThread;

class GraphicsTracingWidget : public BreakPointObserverDock {
    Q_OBJECT

public:
    GraphicsTracingWidget(std::shared_ptr<Pica::DebugContext> debug_context, QWidget* parent = nullptr);

private slots:
    void StartRecording();
    void StopRecording();
    void AbortRecording();

    void OnBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnResumed() override;

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

signals:
    void SetStartTracingButtonEnabled(bool enable);
    void SetStopTracingButtonEnabled(bool enable);
    void SetAbortTracingButtonEnabled(bool enable);
};
