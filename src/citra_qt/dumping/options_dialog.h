// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>
#include <QDialog>
#include "common/param_package.h"
#include "core/dumping/ffmpeg_backend.h"

namespace Ui {
class OptionsDialog;
}

class OptionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptionsDialog(QWidget* parent, std::vector<VideoDumper::OptionInfo> options,
                           const std::string& current_value);
    ~OptionsDialog() override;

    std::string GetCurrentValue() const;

private:
    void PopulateOptions(const std::string& current_value);
    void OnSetOptionValue(int id);

    std::unique_ptr<Ui::OptionsDialog> ui;
    std::vector<VideoDumper::OptionInfo> options;
    Common::ParamPackage current_values;
};
