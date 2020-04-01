// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QTimer>
#include "citra_qt/configuration/configure_touch_from_button.h"
#include "common/param_package.h"
#include "input_common/main.h"
#include "ui_configure_touch_from_button.h"

static QString GetKeyName(int key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
        return QString{};
    default:
        return QKeySequence(key_code).toString();
    }
}

static QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "keyboard") {
        return GetKeyName(param.Get("code", 0));
    }

    if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat")) {
            const QString hat_str = QString::fromStdString(param.Get("hat", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Hat %1 %2").arg(hat_str, direction_str);
        }

        if (param.Has("axis")) {
            const QString axis_str = QString::fromStdString(param.Get("axis", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Axis %1%2").arg(axis_str, direction_str);
        }

        if (param.Has("button")) {
            const QString button_str = QString::fromStdString(param.Get("button", ""));

            return QObject::tr("Button %1").arg(button_str);
        }

        return {};
    }

    return QObject::tr("[unknown]");
}

ConfigureTouchFromButton::ConfigureTouchFromButton(
    QWidget* parent, std::vector<Settings::TouchFromButtonMap> touch_maps, int default_index)
    : QDialog(parent), touch_maps(touch_maps), selected_index(default_index),
      ui(std::make_unique<Ui::ConfigureTouchFromButton>()),
      timeout_timer(std::make_unique<QTimer>()), poll_timer(std::make_unique<QTimer>()) {

    ui->setupUi(this);
    binding_list_model = std::make_unique<QStandardItemModel>(0, 3, this);
    binding_list_model->setHorizontalHeaderLabels({tr("Button"), tr("X"), tr("Y")});
    ui->binding_list->setModel(binding_list_model.get());

    SetConfiguration();
    UpdateUiDisplay();
    ConnectEvents();
}

ConfigureTouchFromButton::~ConfigureTouchFromButton() = default;

void ConfigureTouchFromButton::showEvent(QShowEvent* ev) {
    QWidget::showEvent(ev);

    // width values are not valid in the constructor
    const int w = ui->binding_list->contentsRect().width() / binding_list_model->columnCount();
    if (w > 0) {
        ui->binding_list->setColumnWidth(0, w);
        ui->binding_list->setColumnWidth(1, w);
        ui->binding_list->setColumnWidth(2, w);
    }
}

void ConfigureTouchFromButton::SetConfiguration() {
    for (const auto& touch_map : touch_maps) {
        ui->mapping->addItem(QString::fromStdString(touch_map.name));
    }

    ui->mapping->setCurrentIndex(selected_index);
}

void ConfigureTouchFromButton::UpdateUiDisplay() {
    const bool have_maps = !touch_maps.empty();

    ui->button_delete->setEnabled(touch_maps.size() > 1);
    ui->button_rename->setEnabled(have_maps);
    ui->binding_list->setEnabled(have_maps);
    ui->button_add_bind->setEnabled(have_maps);
    ui->button_delete_bind->setEnabled(false);

    binding_list_model->removeRows(0, binding_list_model->rowCount());

    if (!have_maps) {
        return;
    }

    for (const auto& button_str : touch_maps[selected_index].buttons) {
        Common::ParamPackage package{button_str};
        QStandardItem* button = new QStandardItem(ButtonToText(package));
        button->setData(QString::fromStdString(button_str));
        button->setEditable(false);
        QStandardItem* xcoord = new QStandardItem(QString::number(package.Get("x", 0)));
        QStandardItem* ycoord = new QStandardItem(QString::number(package.Get("y", 0)));
        binding_list_model->appendRow({button, xcoord, ycoord});
    }
}

void ConfigureTouchFromButton::ConnectEvents() {
    connect(ui->mapping, qOverload<int>(&QComboBox::activated), this, [this](int index) {
        SaveCurrentMapping();
        selected_index = index;
        UpdateUiDisplay();
    });
    connect(ui->button_new, &QPushButton::clicked, this, &ConfigureTouchFromButton::NewMapping);
    connect(ui->button_delete, &QPushButton::clicked, this,
            &ConfigureTouchFromButton::DeleteMapping);
    connect(ui->button_rename, &QPushButton::clicked, this,
            &ConfigureTouchFromButton::RenameMapping);
    connect(ui->button_add_bind, &QPushButton::clicked, this,
            &ConfigureTouchFromButton::NewBinding);
    connect(ui->button_delete_bind, &QPushButton::clicked, this,
            &ConfigureTouchFromButton::DeleteBinding);
    connect(ui->binding_list, &QTreeView::doubleClicked, this,
            &ConfigureTouchFromButton::EditBinding);
    connect(ui->binding_list->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this](const QItemSelection& selected, const QItemSelection& deselected) {
                ui->button_delete_bind->setEnabled(!selected.indexes().isEmpty());
            });
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this,
            &ConfigureTouchFromButton::ApplyConfiguration);

    connect(timeout_timer.get(), &QTimer::timeout, [this]() { SetPollingResult({}, true); });

    connect(poll_timer.get(), &QTimer::timeout, [this]() {
        Common::ParamPackage params;
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            if (params.Has("engine")) {
                SetPollingResult(params, false);
                return;
            }
        }
    });
}

void ConfigureTouchFromButton::SaveCurrentMapping() {
    auto& map = touch_maps[selected_index];
    map.buttons.clear();
    for (int i = 0, rc = binding_list_model->rowCount(); i < rc; ++i) {
        auto bind_str = binding_list_model->index(i, 0)
                            .data(Qt::ItemDataRole::UserRole + 1)
                            .toString()
                            .toStdString();
        if (bind_str.empty()) {
            continue;
        }
        Common::ParamPackage params{bind_str};
        if (!params.Has("engine")) {
            continue;
        }
        params.Set("x", binding_list_model->index(i, 1).data().toInt());
        params.Set("y", binding_list_model->index(i, 2).data().toInt());
        map.buttons.emplace_back(params.Serialize());
    }
}

void ConfigureTouchFromButton::NewMapping() {
    const QString name =
        QInputDialog::getText(this, tr("New Profile"), tr("Enter the name for the new profile."));
    if (name.isEmpty()) {
        return;
    }

    if (selected_index > 0) {
        SaveCurrentMapping();
    }
    touch_maps.emplace_back(Settings::TouchFromButtonMap{name.toStdString(), {}});
    selected_index = touch_maps.size() - 1;

    ui->mapping->addItem(name);
    ui->mapping->setCurrentIndex(selected_index);
    UpdateUiDisplay();
}

void ConfigureTouchFromButton::DeleteMapping() {
    const auto answer = QMessageBox::question(
        this, tr("Delete Profile"), tr("Delete profile %1?").arg(ui->mapping->currentText()));
    if (answer != QMessageBox::Yes) {
        return;
    }
    ui->mapping->removeItem(selected_index);
    ui->mapping->setCurrentIndex(0);
    touch_maps.erase(touch_maps.begin() + selected_index);
    selected_index = touch_maps.size() ? 0 : -1;
    UpdateUiDisplay();
}

void ConfigureTouchFromButton::RenameMapping() {
    const QString new_name = QInputDialog::getText(this, tr("Rename Profile"), tr("New name:"));
    if (new_name.isEmpty()) {
        return;
    }
    ui->mapping->setItemText(selected_index, new_name);
    touch_maps[selected_index].name = new_name.toStdString();
}

void ConfigureTouchFromButton::GetButtonInput(int row_index, bool is_new) {
    binding_list_model->item(row_index, 0)->setText(tr("[press key]"));

    input_setter = [this, row_index, is_new](const Common::ParamPackage& params,
                                             const bool cancel) {
        auto cell = binding_list_model->item(row_index, 0);
        if (!cancel) {
            cell->setText(ButtonToText(params));
            cell->setData(QString::fromStdString(params.Serialize()));
        } else {
            if (is_new) {
                binding_list_model->removeRow(row_index);
            } else {
                cell->setText(
                    ButtonToText(Common::ParamPackage{cell->data().toString().toStdString()}));
            }
        }
    };

    device_pollers = InputCommon::Polling::GetPollers(InputCommon::Polling::DeviceType::Button);

    for (auto& poller : device_pollers) {
        poller->Start();
    }

    grabKeyboard();
    grabMouse();
    timeout_timer->start(5000); // Cancel after 5 seconds
    poll_timer->start(200);     // Check for new inputs every 200ms
}

void ConfigureTouchFromButton::NewBinding() {
    QStandardItem* button = new QStandardItem();
    button->setEditable(false);
    binding_list_model->appendRow(
        {button, new QStandardItem(QStringLiteral("0")), new QStandardItem(QStringLiteral("0"))});
    ui->binding_list->setFocus();
    ui->binding_list->setCurrentIndex(button->index());

    GetButtonInput(binding_list_model->rowCount() - 1, true);
}

void ConfigureTouchFromButton::EditBinding(const QModelIndex& qi) {
    if (qi.row() >= 0 && qi.column() == 0) {
        GetButtonInput(qi.row(), false);
    }
}

void ConfigureTouchFromButton::DeleteBinding() {
    const int row_index = ui->binding_list->currentIndex().row();
    if (row_index >= 0) {
        binding_list_model->removeRow(row_index);
    }
}

void ConfigureTouchFromButton::SetPollingResult(const Common::ParamPackage& params, bool cancel) {
    releaseKeyboard();
    releaseMouse();
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers) {
        poller->Stop();
    }
    if (input_setter) {
        (*input_setter)(params, cancel);
        input_setter.reset();
    }
}

void ConfigureTouchFromButton::keyPressEvent(QKeyEvent* event) {
    if (!input_setter || !event)
        return QDialog::keyPressEvent(event);

    if (event->key() != Qt::Key_Escape) {
        SetPollingResult(Common::ParamPackage{InputCommon::GenerateKeyboardParam(event->key())},
                         false);
    } else {
        SetPollingResult({}, true);
    }
}

void ConfigureTouchFromButton::ApplyConfiguration() {
    SaveCurrentMapping();
    accept();
}

const int ConfigureTouchFromButton::GetSelectedIndex() {
    return selected_index;
}

const std::vector<Settings::TouchFromButtonMap> ConfigureTouchFromButton::GetMaps() {
    return touch_maps;
};