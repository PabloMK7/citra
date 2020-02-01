// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QTreeWidgetItem>
#include "citra_qt/dumping/option_set_dialog.h"
#include "citra_qt/dumping/options_dialog.h"
#include "ui_options_dialog.h"

constexpr char UNSET_TEXT[] = QT_TR_NOOP("[not set]");

void OptionsDialog::PopulateOptions(const std::string& current_value) {
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& option = options.at(i);
        auto* item = new QTreeWidgetItem(
            {QString::fromStdString(option.name), QString::fromStdString(current_values.Get(
                                                      option.name, tr(UNSET_TEXT).toStdString()))});
        item->setData(1, Qt::UserRole, static_cast<unsigned long long>(i)); // ID
        ui->main->addTopLevelItem(item);
    }
}

void OptionsDialog::OnSetOptionValue(int id) {
    OptionSetDialog dialog(this, options[id],
                           current_values.Get(options[id].name, options[id].default_value));
    if (dialog.exec() != QDialog::DialogCode::Accepted) {
        return;
    }

    const auto& [is_set, value] = dialog.GetCurrentValue();
    if (is_set) {
        current_values.Set(options[id].name, value);
    } else {
        current_values.Erase(options[id].name);
    }
    ui->main->invisibleRootItem()->child(id)->setText(1, is_set ? QString::fromStdString(value)
                                                                : tr(UNSET_TEXT));
}

std::string OptionsDialog::GetCurrentValue() const {
    return current_values.Serialize();
}

OptionsDialog::OptionsDialog(QWidget* parent, std::vector<VideoDumper::OptionInfo> options_,
                             const std::string& current_value)
    : QDialog(parent), ui(std::make_unique<Ui::OptionsDialog>()), options(std::move(options_)),
      current_values(current_value) {

    ui->setupUi(this);
    PopulateOptions(current_value);

    connect(ui->main, &QTreeWidget::itemDoubleClicked, [this](QTreeWidgetItem* item, int column) {
        OnSetOptionValue(item->data(1, Qt::UserRole).toInt());
    });
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &OptionsDialog::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &OptionsDialog::reject);
}

OptionsDialog::~OptionsDialog() = default;
