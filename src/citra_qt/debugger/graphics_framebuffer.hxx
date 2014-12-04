// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QDockWidget>

#include "video_core/debug_utils/debug_utils.h"

class QComboBox;
class QLabel;
class QSpinBox;

class CSpinBox;

// Utility class which forwards calls to OnPicaBreakPointHit and OnPicaResume to public slots.
// This is because the Pica breakpoint callbacks are called from a non-GUI thread, while
// the widget usually wants to perform reactions in the GUI thread.
class BreakPointObserverDock : public QDockWidget, Pica::DebugContext::BreakPointObserver {
    Q_OBJECT

public:
    BreakPointObserverDock(std::shared_ptr<Pica::DebugContext> debug_context, const QString& title,
                           QWidget* parent = nullptr);

    void OnPicaBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnPicaResume() override;

private slots:
    virtual void OnBreakPointHit(Pica::DebugContext::Event event, void* data) = 0;
    virtual void OnResumed() = 0;

signals:
    void Resumed();
    void BreakPointHit(Pica::DebugContext::Event event, void* data);
};

class GraphicsFramebufferWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

    enum class Source {
        PicaTarget = 0,
        Custom = 1,

        // TODO: Add GPU framebuffer sources!
    };

    enum class Format {
        RGBA8    = 0,
        RGB8     = 1,
        RGBA5551 = 2,
        RGB565   = 3,
        RGBA4    = 4,
    };

public:
    GraphicsFramebufferWidget(std::shared_ptr<Pica::DebugContext> debug_context, QWidget* parent = nullptr);

public slots:
    void OnFramebufferSourceChanged(int new_value);
    void OnFramebufferAddressChanged(qint64 new_value);
    void OnFramebufferWidthChanged(int new_value);
    void OnFramebufferHeightChanged(int new_value);
    void OnFramebufferFormatChanged(int new_value);
    void OnUpdate();

private slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnResumed() override;

signals:
    void Update();

private:

    QComboBox* framebuffer_source_list;
    CSpinBox* framebuffer_address_control;
    QSpinBox* framebuffer_width_control;
    QSpinBox* framebuffer_height_control;
    QComboBox* framebuffer_format_control;

    QLabel* framebuffer_picture_label;

    Source framebuffer_source;
    unsigned framebuffer_address;
    unsigned framebuffer_width;
    unsigned framebuffer_height;
    Format framebuffer_format;
};
