// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QString>
#include <QVBoxLayout>
#include "citra_qt/applets/mii_selector.h"
#include "common/file_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/applets/buttons.h"
#include "core/hle/service/ptm/ptm.h"

QtMiiSelectorDialog::QtMiiSelectorDialog(QWidget* parent, QtMiiSelector* mii_selector_)
    : QDialog(parent), mii_selector(mii_selector_) {
    Frontend::MiiSelectorConfig config = mii_selector->config;
    layout = new QVBoxLayout;
    combobox = new QComboBox;
    buttons = new QDialogButtonBox;
    // Initialize buttons
    buttons->addButton(tr(AppletButton::Ok), QDialogButtonBox::ButtonRole::AcceptRole);
    if (config.enable_cancel_button) {
        buttons->addButton(tr(AppletButton::Cancel), QDialogButtonBox::ButtonRole::RejectRole);
    }

    setWindowTitle(config.title.empty() ? tr("Mii Selector")
                                        : QString::fromStdU16String(config.title));

    std::string nand_directory{FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id);
    if (!archive_result.Succeeded()) {
        ShowNoMiis();
        return;
    }

    auto archive = std::move(archive_result).Unwrap();

    FileSys::Path file_path = "/CFL_DB.dat";
    FileSys::Mode mode{};
    mode.read_flag.Assign(1);

    auto file_result = archive->OpenFile(file_path, mode);
    if (!file_result.Succeeded()) {
        ShowNoMiis();
        return;
    }

    auto file = std::move(file_result).Unwrap();

    u32 saved_miis_offset = 0x8;
    // The Mii Maker has a 100 Mii limit on the 3ds
    for (int i = 0; i < 100; ++i) {
        HLE::Applets::MiiData mii;
        std::array<u8, sizeof(mii)> mii_raw;
        file->Read(saved_miis_offset, sizeof(mii), mii_raw.data());
        std::memcpy(&mii, mii_raw.data(), sizeof(mii));
        if (mii.mii_id != 0) {
            std::u16string name(sizeof(mii.mii_name), '\0');
            std::memcpy(&name[0], mii.mii_name.data(), sizeof(mii.mii_name));
            miis.emplace(combobox->count(), mii);
            combobox->addItem(QString::fromStdU16String(name));
        }
        saved_miis_offset += sizeof(mii);
    }

    if (miis.empty()) {
        ShowNoMiis();
        return;
    }

    if (combobox->count() > static_cast<int>(config.initially_selected_mii_index)) {
        combobox->setCurrentIndex(static_cast<int>(config.initially_selected_mii_index));
    }

    connect(buttons, &QDialogButtonBox::accepted, this, [this] { accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, [this] {
        return_code = 1;
        accept();
    });
    layout->addWidget(combobox);
    layout->addWidget(buttons);
    setLayout(layout);
}

void QtMiiSelectorDialog::ShowNoMiis() {
    Frontend::MiiSelectorConfig config = mii_selector->config;
    QMessageBox::StandardButton answer = QMessageBox::question(
        nullptr,
        config.title.empty() ? tr("Mii Selector") : QString::fromStdU16String(config.title),
        tr("You don't have any Miis.\nUse standard Mii?"), QMessageBox::Yes | QMessageBox::No);

    if (answer == QMessageBox::No)
        return_code = 1;

    QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
}

QtMiiSelector::QtMiiSelector(QWidget& parent_) : parent(parent_) {}

void QtMiiSelector::Setup(const Frontend::MiiSelectorConfig* config) {
    MiiSelector::Setup(config);
    QMetaObject::invokeMethod(this, "OpenDialog", Qt::BlockingQueuedConnection);
}

void QtMiiSelector::OpenDialog() {
    QtMiiSelectorDialog dialog(&parent, this);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    int index = dialog.combobox->currentIndex();
    LOG_INFO(Frontend, "Mii Selector dialog finished (return_code={}, index={})",
             dialog.return_code, index);

    HLE::Applets::MiiData mii_data =
        index == -1 ? HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data
                    : dialog.miis.at(dialog.combobox->currentIndex());
    Finalize(dialog.return_code, dialog.return_code == 0 ? mii_data : HLE::Applets::MiiData{});
}
