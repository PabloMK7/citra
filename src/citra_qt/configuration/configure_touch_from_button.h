// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <QDialog>
#include "core/settings.h"

class QKeyEvent;
class QModelIndex;
class QStandardItemModel;
class QTimer;

namespace Common {
class ParamPackage;
}

namespace InputCommon {
namespace Polling {
class DevicePoller;
}
} // namespace InputCommon

namespace Ui {
class ConfigureTouchFromButton;
}

class ConfigureTouchFromButton : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureTouchFromButton(QWidget* parent,
                                      std::vector<Settings::TouchFromButtonMap> touch_maps,
                                      int default_index = 0);
    ~ConfigureTouchFromButton() override;

    const int GetSelectedIndex();
    const std::vector<Settings::TouchFromButtonMap> GetMaps();

public slots:
    void ApplyConfiguration();

protected:
    void showEvent(QShowEvent* ev);

private:
    void SetConfiguration();
    void UpdateUiDisplay();
    void ConnectEvents();
    void NewMapping();
    void DeleteMapping();
    void RenameMapping();
    void NewBinding();
    void EditBinding(const QModelIndex& qi);
    void DeleteBinding();
    void GetButtonInput(int row_index, bool is_new);
    void SetPollingResult(const Common::ParamPackage& params, bool cancel);
    void SaveCurrentMapping();
    void keyPressEvent(QKeyEvent* event) override;

    std::unique_ptr<Ui::ConfigureTouchFromButton> ui;
    std::unique_ptr<QStandardItemModel> binding_list_model;
    std::vector<Settings::TouchFromButtonMap> touch_maps;
    int selected_index;

    std::unique_ptr<QTimer> timeout_timer;
    std::unique_ptr<QTimer> poll_timer;
    std::vector<std::unique_ptr<InputCommon::Polling::DevicePoller>> device_pollers;
    std::optional<std::function<void(const Common::ParamPackage&, const bool)>> input_setter;
};
