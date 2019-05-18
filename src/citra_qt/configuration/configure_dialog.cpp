// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <QListWidgetItem>
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_dialog.h"
#include "citra_qt/hotkeys.h"
#include "core/settings.h"
#include "ui_configure.h"

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry, bool enable_web_config)
    : QDialog(parent), ui(new Ui::ConfigureDialog), registry(registry) {
    ui->setupUi(this);
    ui->hotkeysTab->Populate(registry);
    ui->webTab->SetWebServiceConfigEnabled(enable_web_config);

    this->PopulateSelectionList();

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(ui->uiTab, &ConfigureUi::languageChanged, this, &ConfigureDialog::onLanguageChanged);
    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    adjustSize();
    ui->selectorList->setCurrentRow(0);

    // Set up used key list synchronisation
    connect(ui->inputTab, &ConfigureInput::InputKeysChanged, ui->hotkeysTab,
            &ConfigureHotkeys::OnInputKeysChanged);
    connect(ui->hotkeysTab, &ConfigureHotkeys::HotkeysChanged, ui->inputTab,
            &ConfigureInput::OnHotkeysChanged);

    // Synchronise lists upon initialisation
    ui->inputTab->EmitInputKeysChanged();
    ui->hotkeysTab->EmitHotkeysChanged();
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::setConfiguration() {
    ui->generalTab->setConfiguration();
    ui->systemTab->setConfiguration();
    ui->inputTab->loadConfiguration();
    ui->graphicsTab->setConfiguration();
    ui->audioTab->setConfiguration();
    ui->cameraTab->setConfiguration();
    ui->debugTab->setConfiguration();
    ui->webTab->setConfiguration();
    ui->uiTab->setConfiguration();
}

void ConfigureDialog::applyConfiguration() {
    ui->generalTab->applyConfiguration();
    ui->systemTab->applyConfiguration();
    ui->inputTab->applyConfiguration();
    ui->inputTab->ApplyProfile();
    ui->hotkeysTab->applyConfiguration(registry);
    ui->graphicsTab->applyConfiguration();
    ui->audioTab->applyConfiguration();
    ui->cameraTab->applyConfiguration();
    ui->debugTab->applyConfiguration();
    ui->webTab->applyConfiguration();
    ui->uiTab->applyConfiguration();
    Settings::Apply();
    Settings::LogSettings();
}

void ConfigureDialog::PopulateSelectionList() {
    ui->selectorList->clear();

    const std::array<std::pair<QString, QStringList>, 4> items{
        {{tr("General"),
          {QT_TR_NOOP("General"), QT_TR_NOOP("Web"), QT_TR_NOOP("Debug"), QT_TR_NOOP("UI")}},
         {tr("System"), {QT_TR_NOOP("System"), QT_TR_NOOP("Audio"), QT_TR_NOOP("Camera")}},
         {tr("Graphics"), {QT_TR_NOOP("Graphics")}},
         {tr("Controls"), {QT_TR_NOOP("Input"), QT_TR_NOOP("Hotkeys")}}}};

    for (const auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, entry.second);

        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::onLanguageChanged(const QString& locale) {
    emit languageChanged(locale);
    // first apply the configuration, and then restore the display
    applyConfiguration();
    retranslateUi();
    setConfiguration();
}

void ConfigureDialog::retranslateUi() {
    int old_row = ui->selectorList->currentRow();
    int old_index = ui->tabWidget->currentIndex();
    ui->retranslateUi(this);
    PopulateSelectionList();
    // restore selection after repopulating
    ui->selectorList->setCurrentRow(old_row);
    ui->tabWidget->setCurrentIndex(old_index);

    ui->generalTab->retranslateUi();
    ui->systemTab->retranslateUi();
    ui->inputTab->retranslateUi();
    ui->hotkeysTab->retranslateUi();
    ui->graphicsTab->retranslateUi();
    ui->audioTab->retranslateUi();
    ui->cameraTab->retranslateUi();
    ui->debugTab->retranslateUi();
    ui->webTab->retranslateUi();
    ui->uiTab->retranslateUi();
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty())
        return;

    const std::map<QString, QWidget*> widgets = {
        {"General", ui->generalTab},   {"System", ui->systemTab},
        {"Input", ui->inputTab},       {"Hotkeys", ui->hotkeysTab},
        {"Graphics", ui->graphicsTab}, {"Audio", ui->audioTab},
        {"Camera", ui->cameraTab},     {"Debug", ui->debugTab},
        {"Web", ui->webTab},           {"UI", ui->uiTab}};

    ui->tabWidget->clear();

    const QStringList tabs = items[0]->data(Qt::UserRole).toStringList();

    for (const auto& tab : tabs)
        ui->tabWidget->addTab(widgets.at(tab), tr(qPrintable(tab)));
}
