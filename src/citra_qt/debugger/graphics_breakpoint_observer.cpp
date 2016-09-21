// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMetaType>
#include "citra_qt/debugger/graphics_breakpoint_observer.h"

BreakPointObserverDock::BreakPointObserverDock(std::shared_ptr<Pica::DebugContext> debug_context,
                                               const QString& title, QWidget* parent)
    : QDockWidget(title, parent), BreakPointObserver(debug_context) {
    qRegisterMetaType<Pica::DebugContext::Event>("Pica::DebugContext::Event");

    connect(this, SIGNAL(Resumed()), this, SLOT(OnResumed()));

    // NOTE: This signal is emitted from a non-GUI thread, but connect() takes
    //       care of delaying its handling to the GUI thread.
    connect(this, SIGNAL(BreakPointHit(Pica::DebugContext::Event, void*)), this,
            SLOT(OnBreakPointHit(Pica::DebugContext::Event, void*)), Qt::BlockingQueuedConnection);
}

void BreakPointObserverDock::OnPicaBreakPointHit(Pica::DebugContext::Event event, void* data) {
    emit BreakPointHit(event, data);
}

void BreakPointObserverDock::OnPicaResume() {
    emit Resumed();
}
