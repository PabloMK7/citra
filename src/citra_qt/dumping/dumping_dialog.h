// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QDialog>
#include "core/dumping/ffmpeg_backend.h"

namespace Ui {
class DumpingDialog;
}

class DumpingDialog : public QDialog {
    Q_OBJECT

public:
    explicit DumpingDialog(QWidget* parent);
    ~DumpingDialog() override;

    QString GetFilePath() const;
    void ApplyConfiguration();

private:
    void Populate();
    void PopulateEncoders();
    void SetConfiguration();
    void OnToolButtonClicked();
    void OpenOptionsDialog(const std::vector<VideoDumper::OptionInfo>& options,
                           std::string& current_value);

    std::unique_ptr<Ui::DumpingDialog> ui;
    std::string format_options;
    std::string video_encoder_options;
    std::string audio_encoder_options;

    QString last_path;

    std::vector<VideoDumper::FormatInfo> formats;
    std::vector<VideoDumper::EncoderInfo> video_encoders;
    std::vector<VideoDumper::EncoderInfo> audio_encoders;
};
