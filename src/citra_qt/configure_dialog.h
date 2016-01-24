// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef CONFIGURE_DIALOG_H
#define CONFIGURE_DIALOG_H

#include <QDialog>

namespace Ui {
class ConfigureDialog;
}

class ConfigureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigureDialog(QWidget *parent = 0);
    ~ConfigureDialog();

    void applyConfiguration();

private:
    void setConfiguration();

private:
    Ui::ConfigureDialog *ui;
};

#endif // CONFIGURE_DIALOG_H
