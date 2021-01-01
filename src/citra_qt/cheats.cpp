// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCheckBox>
#include <QMessageBox>
#include <QTableWidgetItem>
#include "citra_qt/cheats.h"
#include "core/cheats/cheat_base.h"
#include "core/cheats/cheats.h"
#include "core/cheats/gateway_cheat.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "ui_cheats.h"

CheatDialog::CheatDialog(QWidget* parent)
    : QDialog(parent), ui(std::make_unique<Ui::CheatDialog>()) {
    // Setup gui control settings
    ui->setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    ui->tableCheats->setColumnWidth(0, 30);
    ui->tableCheats->setColumnWidth(2, 85);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->tableCheats->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    ui->lineName->setEnabled(false);
    ui->textCode->setEnabled(false);
    ui->textNotes->setEnabled(false);
    const auto game_id = fmt::format(
        "{:016X}", Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);
    ui->labelTitle->setText(tr("Title ID: %1").arg(QString::fromStdString(game_id)));

    connect(ui->buttonClose, &QPushButton::clicked, this, &CheatDialog::OnCancel);
    connect(ui->buttonAddCheat, &QPushButton::clicked, this, &CheatDialog::OnAddCheat);
    connect(ui->tableCheats, &QTableWidget::cellClicked, this, &CheatDialog::OnRowSelected);
    connect(ui->lineName, &QLineEdit::textEdited, this, &CheatDialog::OnTextEdited);
    connect(ui->textNotes, &QPlainTextEdit::textChanged, this, &CheatDialog::OnTextEdited);
    connect(ui->textCode, &QPlainTextEdit::textChanged, this, &CheatDialog::OnTextEdited);

    connect(ui->buttonSave, &QPushButton::clicked,
            [this] { SaveCheat(ui->tableCheats->currentRow()); });
    connect(ui->buttonDelete, &QPushButton::clicked, this, &CheatDialog::OnDeleteCheat);

    LoadCheats();
}

CheatDialog::~CheatDialog() = default;

void CheatDialog::LoadCheats() {
    cheats = Core::System::GetInstance().CheatEngine().GetCheats();

    ui->tableCheats->setRowCount(cheats.size());

    for (size_t i = 0; i < cheats.size(); i++) {
        QCheckBox* enabled = new QCheckBox();
        enabled->setChecked(cheats[i]->IsEnabled());
        enabled->setStyleSheet(QStringLiteral("margin-left:7px;"));
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

bool CheatDialog::CheckSaveCheat() {
    auto answer = QMessageBox::warning(
        this, tr("Cheats"), tr("Would you like to save the current cheat?"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Cancel);

    if (answer == QMessageBox::Yes) {
        return SaveCheat(last_row);
    } else {
        return answer != QMessageBox::Cancel;
    }
}

bool CheatDialog::SaveCheat(int row) {
    if (ui->lineName->text().isEmpty()) {
        QMessageBox::critical(this, tr("Save Cheat"), tr("Please enter a cheat name."));
        return false;
    }
    if (ui->textCode->toPlainText().isEmpty()) {
        QMessageBox::critical(this, tr("Save Cheat"), tr("Please enter the cheat code."));
        return false;
    }

    // Check if the cheat lines are valid
    auto code_lines = ui->textCode->toPlainText().split(QLatin1Char{'\n'}, QString::SkipEmptyParts);
    for (int i = 0; i < code_lines.size(); ++i) {
        Cheats::GatewayCheat::CheatLine cheat_line(code_lines[i].toStdString());
        if (cheat_line.valid)
            continue;

        auto answer = QMessageBox::warning(
            this, tr("Save Cheat"),
            tr("Cheat code line %1 is not valid.\nWould you like to ignore the error and continue?")
                .arg(i + 1),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer == QMessageBox::No)
            return false;
    }

    auto cheat = std::make_shared<Cheats::GatewayCheat>(ui->lineName->text().toStdString(),
                                                        ui->textCode->toPlainText().toStdString(),
                                                        ui->textNotes->toPlainText().toStdString());

    if (newly_created) {
        Core::System::GetInstance().CheatEngine().AddCheat(cheat);
        newly_created = false;
    } else {
        Core::System::GetInstance().CheatEngine().UpdateCheat(row, cheat);
    }
    Core::System::GetInstance().CheatEngine().SaveCheatFile();

    int previous_row = ui->tableCheats->currentRow();
    int previous_col = ui->tableCheats->currentColumn();
    LoadCheats();
    ui->tableCheats->setCurrentCell(previous_row, previous_col);

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonAddCheat->setEnabled(true);
    return true;
}

void CheatDialog::closeEvent(QCloseEvent* event) {
    if (edited && !CheckSaveCheat()) {
        event->ignore();
        return;
    }
    event->accept();
}

void CheatDialog::OnCancel() {
    close();
}

void CheatDialog::OnRowSelected(int row, int column) {
    if (row == last_row) {
        return;
    }
    if (edited && !CheckSaveCheat()) {
        ui->tableCheats->setCurrentCell(last_row, last_col);
        return;
    }
    if (static_cast<std::size_t>(row) < cheats.size()) {
        if (newly_created) {
            // Remove the newly created dummy item
            newly_created = false;
            ui->tableCheats->setRowCount(ui->tableCheats->rowCount() - 1);
        }

        const auto& current_cheat = cheats[row];
        ui->lineName->setText(QString::fromStdString(current_cheat->GetName()));
        ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
        ui->textCode->setPlainText(QString::fromStdString(current_cheat->GetCode()));
    }

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonDelete->setEnabled(true);
    ui->buttonAddCheat->setEnabled(true);
    ui->lineName->setEnabled(true);
    ui->textCode->setEnabled(true);
    ui->textNotes->setEnabled(true);

    last_row = row;
    last_col = column;
}

void CheatDialog::OnCheckChanged(int state) {
    const QCheckBox* checkbox = qobject_cast<QCheckBox*>(sender());
    int row = static_cast<int>(checkbox->property("row").toInt());
    cheats[row]->SetEnabled(state);
    Core::System::GetInstance().CheatEngine().SaveCheatFile();
}

void CheatDialog::OnTextEdited() {
    edited = true;
    ui->buttonSave->setEnabled(true);
}

void CheatDialog::OnDeleteCheat() {
    if (newly_created) {
        newly_created = false;
    } else {
        Core::System::GetInstance().CheatEngine().RemoveCheat(ui->tableCheats->currentRow());
        Core::System::GetInstance().CheatEngine().SaveCheatFile();
    }

    LoadCheats();
    if (cheats.empty()) {
        ui->lineName->clear();
        ui->textCode->clear();
        ui->textNotes->clear();
        ui->lineName->setEnabled(false);
        ui->textCode->setEnabled(false);
        ui->textNotes->setEnabled(false);
        ui->buttonDelete->setEnabled(false);
        last_row = last_col = -1;
    } else {
        if (last_row >= ui->tableCheats->rowCount()) {
            last_row = ui->tableCheats->rowCount() - 1;
        }
        ui->tableCheats->setCurrentCell(last_row, last_col);

        const auto& current_cheat = cheats[last_row];
        ui->lineName->setText(QString::fromStdString(current_cheat->GetName()));
        ui->textNotes->setPlainText(QString::fromStdString(current_cheat->GetComments()));
        ui->textCode->setPlainText(QString::fromStdString(current_cheat->GetCode()));
    }

    edited = false;
    ui->buttonSave->setEnabled(false);
    ui->buttonAddCheat->setEnabled(true);
}

void CheatDialog::OnAddCheat() {
    if (edited && !CheckSaveCheat()) {
        return;
    }

    int row = ui->tableCheats->rowCount();
    ui->tableCheats->setRowCount(row + 1);
    ui->tableCheats->setCurrentCell(row, 1);

    // create a dummy item
    ui->tableCheats->setItem(row, 1, new QTableWidgetItem(tr("[new cheat]")));
    ui->tableCheats->setItem(row, 2, new QTableWidgetItem(QString{}));
    ui->lineName->clear();
    ui->lineName->setPlaceholderText(tr("[new cheat]"));
    ui->textCode->clear();
    ui->textNotes->clear();
    ui->lineName->setEnabled(true);
    ui->textCode->setEnabled(true);
    ui->textNotes->setEnabled(true);
    ui->buttonSave->setEnabled(true);
    ui->buttonDelete->setEnabled(true);
    ui->buttonAddCheat->setEnabled(false);

    edited = false;
    newly_created = true;
    last_row = row;
    last_col = 1;
}
