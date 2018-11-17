// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QTableWidgetItem>
#include "citra_qt/cheats.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "ui_cheats.h"

CheatDialog::CheatDialog(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::CheatDialog>()) {
    // Setup gui control settings
    ui->setupUi(this);
    setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
    ui->tableCheats->setColumnWidth(0, 30);
    ui->tableCheats->setColumnWidth(2, 85);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->textDetails->setEnabled(false);
    ui->textNotes->setEnabled(false);
    const auto game_id = fmt::format(
        "{:016X}", Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);
    ui->labelTitle->setText(tr("Title ID: %1").arg(QString::fromStdString(game_id)));

    connect(ui->buttonClose, &QPushButton::released, this, &CheatDialog::OnCancel);
    connect(ui->tableCheats, &QTableWidget::cellClicked, this, &CheatDialog::OnRowSelected);

    LoadCheats();
}

CheatDialog::~CheatDialog() = default;

void CheatDialog::LoadCheats() {
    const auto& cheats = Core::System::GetInstance().CheatEngine().GetCheats();

    ui->tableCheats->setRowCount(cheats.size());

    for (size_t i = 0; i < cheats.size(); i++) {
        QCheckBox* enabled = new QCheckBox();
        enabled->setChecked(cheats[i]->IsEnabled());
        enabled->setStyleSheet("margin-left:7px;");
        ui->tableCheats->setItem(i, 0, new QTableWidgetItem());
        ui->tableCheats->setCellWidget(i, 0, enabled);
        ui->tableCheats->setItem(
            i, 1, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetName())));
        ui->tableCheats->setItem(
            i, 2, new QTableWidgetItem(QString::fromStdString(cheats[i]->GetType())));
        enabled->setProperty("row", static_cast<int>(i));

        connect(enabled, &QCheckBox::stateChanged, this, &CheatDialog::OnCheckChanged);
    }
}

void CheatDialog::OnCancel() {
    close();
}

void CheatDialog::OnRowSelected(int row, int column) {
    ui->textDetails->setEnabled(true);
    ui->textNotes->setEnabled(true);
    const auto& current_cheat = Core::System::GetInstance().CheatEngine().GetCheats()[row];
    ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
    ui->textDetails->setPlainText(QString::fromStdString(current_cheat->ToString()));
}

void CheatDialog::OnCheckChanged(int state) {
    const QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    int row = static_cast<int>(checkbox->property("row").toInt());
    Core::System::GetInstance().CheatEngine().GetCheats()[row]->SetEnabled(state);
}
