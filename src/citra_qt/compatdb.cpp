// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QButtonGroup>
#include <QMessageBox>
#include <QPushButton>
#include "citra_qt/compatdb.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "ui_compatdb.h"

CompatDB::CompatDB(QWidget* parent)
    : QWizard(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui{std::make_unique<Ui::CompatDB>()} {
    ui->setupUi(this);
    connect(ui->radioButton_Perfect, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Great, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Okay, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_Bad, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_IntroMenu, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(ui->radioButton_WontBoot, &QRadioButton::clicked, this, &CompatDB::EnableNext);
    connect(button(NextButton), &QPushButton::clicked, this, &CompatDB::Submit);
}

CompatDB::~CompatDB() = default;

enum class CompatDBPage {
    Intro = 0,
    Selection = 1,
    Final = 2,
};

void CompatDB::Submit() {
    QButtonGroup* compatibility = new QButtonGroup(this);
    compatibility->addButton(ui->radioButton_Perfect, 0);
    compatibility->addButton(ui->radioButton_Great, 1);
    compatibility->addButton(ui->radioButton_Okay, 2);
    compatibility->addButton(ui->radioButton_Bad, 3);
    compatibility->addButton(ui->radioButton_IntroMenu, 4);
    compatibility->addButton(ui->radioButton_WontBoot, 5);
    switch ((static_cast<CompatDBPage>(currentId()))) {
    case CompatDBPage::Selection:
        if (compatibility->checkedId() == -1) {
            button(NextButton)->setEnabled(false);
        }
        break;
    case CompatDBPage::Final:
        LOG_DEBUG(Frontend, "Compatibility Rating: {}", compatibility->checkedId());
        Core::Telemetry().AddField(Telemetry::FieldType::UserFeedback, "Compatibility",
                                   compatibility->checkedId());
        // older versions of QT don't support the "NoCancelButtonOnLastPage" option, this is a
        // workaround
        button(QWizard::CancelButton)->setVisible(false);
        break;
    default:
        LOG_ERROR(Frontend, "Unexpected page: {}", currentId());
    }
}

void CompatDB::EnableNext() {
    button(NextButton)->setEnabled(true);
}
