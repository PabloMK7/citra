// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include <QBoxLayout>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "core/hw/gpu.h"
#include "video_core/pica.h"

#include "nihstro/float24.h"

#include "graphics_tracing.h"

GraphicsTracingWidget::GraphicsTracingWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                                             QWidget* parent)
    : BreakPointObserverDock(debug_context, tr("CiTrace Recorder"), parent) {

    setObjectName("CiTracing");

    QPushButton* start_recording = new QPushButton(tr("Start Recording"));
    QPushButton* stop_recording = new QPushButton(QIcon::fromTheme("document-save"), tr("Stop and Save"));
    QPushButton* abort_recording = new QPushButton(tr("Abort Recording"));

    connect(this, SIGNAL(SetStartTracingButtonEnabled(bool)), start_recording, SLOT(setVisible(bool)));
    connect(this, SIGNAL(SetStopTracingButtonEnabled(bool)), stop_recording, SLOT(setVisible(bool)));
    connect(this, SIGNAL(SetAbortTracingButtonEnabled(bool)), abort_recording, SLOT(setVisible(bool)));
    connect(start_recording, SIGNAL(clicked()), this, SLOT(StartRecording()));
    connect(stop_recording, SIGNAL(clicked()), this, SLOT(StopRecording()));
    connect(abort_recording, SIGNAL(clicked()), this, SLOT(AbortRecording()));

    stop_recording->setVisible(false);
    abort_recording->setVisible(false);

    auto main_widget = new QWidget;
    auto main_layout = new QVBoxLayout;
    {
        auto sub_layout = new QHBoxLayout;
        sub_layout->addWidget(start_recording);
        sub_layout->addWidget(stop_recording);
        sub_layout->addWidget(abort_recording);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);
    setWidget(main_widget);

    // TODO: Make sure to have this widget disabled as soon as emulation is started!
}

void GraphicsTracingWidget::StartRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    auto shader_binary = Pica::g_state.vs.program_code;
    auto swizzle_data = Pica::g_state.vs.swizzle_data;

    // Encode floating point numbers to 24-bit values
    // TODO: Drop this explicit conversion once we store float24 values bit-correctly internally.
    std::array<Math::Vec4<uint32_t>, 96> vs_float_uniforms;
    for (unsigned i = 0; i < 96; ++i)
        for (unsigned comp = 0; comp < 3; ++comp)
            vs_float_uniforms[i][comp] = nihstro::to_float24(Pica::g_state.vs.uniforms.f[i][comp].ToFloat32());

    auto recorder = new CiTrace::Recorder((u32*)&GPU::g_regs, 0x700, nullptr, 0, (u32*)&Pica::g_state.regs, 0x300,
                                          shader_binary.data(), shader_binary.size(),
                                          swizzle_data.data(), swizzle_data.size(),
                                          (u32*)vs_float_uniforms.data(), vs_float_uniforms.size() * 4,
                                          nullptr, 0, nullptr, 0, nullptr, 0 // Geometry shader is not implemented yet, so submit dummy data for now
                                          );
    context->recorder = std::shared_ptr<CiTrace::Recorder>(recorder);

    emit SetStartTracingButtonEnabled(false);
    emit SetStopTracingButtonEnabled(true);
    emit SetAbortTracingButtonEnabled(true);
}

void GraphicsTracingWidget::StopRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    QString filename = QFileDialog::getSaveFileName(this, tr("Save CiTrace"), "citrace.ctf",
                                                    tr("CiTrace File (*.ctf)"));

    if (filename.isEmpty()) {
        // If the user canceled the dialog, keep recording
        return;
    }

    context->recorder->Finish(filename.toStdString());
    context->recorder = nullptr;

    emit SetStopTracingButtonEnabled(false);
    emit SetAbortTracingButtonEnabled(false);
    emit SetStartTracingButtonEnabled(true);
}

void GraphicsTracingWidget::AbortRecording() {
    auto context = context_weak.lock();
    if (!context)
        return;

    context->recorder = nullptr;

    emit SetStopTracingButtonEnabled(false);
    emit SetAbortTracingButtonEnabled(false);
    emit SetStartTracingButtonEnabled(true);
}

void GraphicsTracingWidget::OnBreakPointHit(Pica::DebugContext::Event event, void* data) {
    widget()->setEnabled(true);
}

void GraphicsTracingWidget::OnResumed() {
    widget()->setEnabled(false);
}
