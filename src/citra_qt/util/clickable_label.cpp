// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/util/clickable_label.h"

ClickableLabel::ClickableLabel(QWidget* parent, Qt::WindowFlags f) : QLabel(parent) {
    Q_UNUSED(f);
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent* event) {
    Q_UNUSED(event);
    emit clicked();
}
