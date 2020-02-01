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
    ui->enhancementsTab->SetConfiguration();
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
    ui->enhancementsTab->ApplyConfiguration();
    ui->audioTab->ApplyConfiguration();
    ui->cameraTab->ApplyConfiguration();
    ui->debugTab->ApplyConfiguration();
    ui->webTab->ApplyConfiguration();
    ui->uiTab->ApplyConfiguration();
    Settings::Apply();
    Settings::LogSettings();
}

Q_DECLARE_METATYPE(QList<QWidget*>);

void ConfigureDialog::PopulateSelectionList() {
    ui->selectorList->clear();

    const std::array<std::pair<QString, QList<QWidget*>>, 5> items{
        {{tr("General"), {ui->generalTab, ui->webTab, ui->debugTab, ui->uiTab}},
         {tr("System"), {ui->systemTab, ui->cameraTab}},
         {tr("Graphics"), {ui->enhancementsTab, ui->graphicsTab}},
         {tr("Audio"), {ui->audioTab}},
         {tr("Controls"), {ui->inputTab, ui->hotkeysTab}}}};

    for (const auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, QVariant::fromValue(entry.second));

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
    ui->enhancementsTab->RetranslateUI();
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

    const std::map<QWidget*, QString> widgets = {{ui->generalTab, tr("General")},
                                                 {ui->systemTab, tr("System")},
                                                 {ui->inputTab, tr("Input")},
                                                 {ui->hotkeysTab, tr("Hotkeys")},
                                                 {ui->enhancementsTab, tr("Enhancements")},
                                                 {ui->graphicsTab, tr("Advanced")},
                                                 {ui->audioTab, tr("Audio")},
                                                 {ui->cameraTab, tr("Camera")},
                                                 {ui->debugTab, tr("Debug")},
                                                 {ui->webTab, tr("Web")},
                                                 {ui->uiTab, tr("UI")}};

    ui->tabWidget->clear();

    const QList<QWidget*> tabs = qvariant_cast<QList<QWidget*>>(items[0]->data(Qt::UserRole));

    for (const auto tab : tabs)
        ui->tabWidget->addTab(tab, widgets.at(tab));
}
