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
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"

QtMiiSelectorDialog::QtMiiSelectorDialog(QWidget* parent, QtMiiSelector* mii_selector_)
    : QDialog(parent), mii_selector(mii_selector_) {
    using namespace Frontend;
    const auto config = mii_selector->config;
    layout = new QVBoxLayout;
    combobox = new QComboBox;
    buttons = new QDialogButtonBox;
    // Initialize buttons
    buttons->addButton(tr(MII_BUTTON_OKAY), QDialogButtonBox::ButtonRole::AcceptRole);
    if (config.enable_cancel_button) {
        buttons->addButton(tr(MII_BUTTON_CANCEL), QDialogButtonBox::ButtonRole::RejectRole);
    }

    setWindowTitle(config.title.empty() || config.title.at(0) == '\x0000'
                       ? tr("Mii Selector")
                       : QString::fromStdString(config.title));

    miis.push_back(HLE::Applets::MiiSelector::GetStandardMiiResult().selected_mii_data);
    combobox->addItem(tr("Standard Mii"));

    std::string nand_directory{FileUtil::GetUserPath(FileUtil::UserPath::NANDDir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory(nand_directory, true);

    auto archive_result = extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id, 0);
    if (archive_result.Succeeded()) {
        auto archive = std::move(archive_result).Unwrap();

        FileSys::Path file_path = "/CFL_DB.dat";
        FileSys::Mode mode{};
        mode.read_flag.Assign(1);

        auto file_result = archive->OpenFile(file_path, mode);
        if (file_result.Succeeded()) {
            auto file = std::move(file_result).Unwrap();

            u32 saved_miis_offset = 0x8;
            // The Mii Maker has a 100 Mii limit on the 3ds
            for (int i = 0; i < 100; ++i) {
                HLE::Applets::MiiData mii;
                std::array<u8, sizeof(mii)> mii_raw;
                file->Read(saved_miis_offset, sizeof(mii), mii_raw.data());
                std::memcpy(&mii, mii_raw.data(), sizeof(mii));
                if (mii.mii_id != 0) {
                    std::string name = Common::UTF16BufferToUTF8(mii.mii_name);
                    miis.push_back(mii);
                    combobox->addItem(QString::fromStdString(name));
                }
                saved_miis_offset += sizeof(mii);
            }
        }
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

QtMiiSelector::QtMiiSelector(QWidget& parent_) : parent(parent_) {}

void QtMiiSelector::Setup(const Frontend::MiiSelectorConfig& config) {
    MiiSelector::Setup(config);
    QMetaObject::invokeMethod(this, "OpenDialog", Qt::BlockingQueuedConnection);
}

void QtMiiSelector::OpenDialog() {
    QtMiiSelectorDialog dialog(&parent, this);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.exec();

    const auto index = dialog.combobox->currentIndex();
    LOG_INFO(Frontend, "Mii Selector dialog finished (return_code={}, index={})",
             dialog.return_code, index);

    const auto mii_data = dialog.miis.at(index);
    Finalize(dialog.return_code,
             dialog.return_code == 0 ? std::move(mii_data) : HLE::Applets::MiiData{});
}
