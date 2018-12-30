// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QVBoxLayout>
#include "citra_qt/util/sequence_dialog/sequence_dialog.h"

SequenceDialog::SequenceDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Enter a hotkey"));
    auto* layout = new QVBoxLayout(this);
    key_sequence = new QKeySequenceEdit;
    layout->addWidget(key_sequence);
    auto* buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal);
    buttons->setCenterButtons(true);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

SequenceDialog::~SequenceDialog() = default;

QKeySequence SequenceDialog::GetSequence() {
    // Only the first key is returned. The other 3, if present, are ignored.
    return QKeySequence(key_sequence->keySequence()[0]);
}

bool SequenceDialog::focusNextPrevChild(bool next) {
    return false;
}

void SequenceDialog::closeEvent(QCloseEvent*) {
    reject();
}
