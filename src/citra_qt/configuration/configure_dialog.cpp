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

    PopulateSelectionList();

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(ui->uiTab, &ConfigureUi::LanguageChanged, this, &ConfigureDialog::OnLanguageChanged);
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

void ConfigureDialog::SetConfiguration() {
    ui->generalTab->SetConfiguration();
    ui->systemTab->SetConfiguration();
    ui->inputTab->LoadConfiguration();
    ui->graphicsTab->SetConfiguration();
    ui->audioTab->SetConfiguration();
    ui->cameraTab->SetConfiguration();
    ui->debugTab->SetConfiguration();
    ui->webTab->SetConfiguration();
    ui->uiTab->SetConfiguration();
}

void ConfigureDialog::ApplyConfiguration() {
    ui->generalTab->ApplyConfiguration();
    ui->systemTab->ApplyConfiguration();
    ui->inputTab->ApplyConfiguration();
    ui->inputTab->ApplyProfile();
    ui->hotkeysTab->ApplyConfiguration(registry);
    ui->graphicsTab->ApplyConfiguration();
    ui->audioTab->ApplyConfiguration();
    ui->cameraTab->ApplyConfiguration();
    ui->debugTab->ApplyConfiguration();
    ui->webTab->ApplyConfiguration();
    ui->uiTab->ApplyConfiguration();
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

void ConfigureDialog::OnLanguageChanged(const QString& locale) {
    emit LanguageChanged(locale);
    // first apply the configuration, and then restore the display
    ApplyConfiguration();
    RetranslateUI();
    SetConfiguration();
}

void ConfigureDialog::RetranslateUI() {
    int old_row = ui->selectorList->currentRow();
    int old_index = ui->tabWidget->currentIndex();
    ui->retranslateUi(this);
    PopulateSelectionList();
    // restore selection after repopulating
    ui->selectorList->setCurrentRow(old_row);
    ui->tabWidget->setCurrentIndex(old_index);

    ui->generalTab->RetranslateUI();
    ui->systemTab->RetranslateUI();
    ui->inputTab->RetranslateUI();
    ui->hotkeysTab->RetranslateUI();
    ui->graphicsTab->RetranslateUI();
    ui->audioTab->RetranslateUI();
    ui->cameraTab->RetranslateUI();
    ui->debugTab->RetranslateUI();
    ui->webTab->RetranslateUI();
    ui->uiTab->RetranslateUI();
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
