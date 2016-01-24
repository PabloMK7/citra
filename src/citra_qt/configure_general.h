// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef CONFIGURE_GENERAL_H
#define CONFIGURE_GENERAL_H

#include <QWidget>

namespace Ui {
class ConfigureGeneral;
}

class ConfigureGeneral : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigureGeneral(QWidget *parent = 0);
    ~ConfigureGeneral();

    void applyConfiguration();

private:
    void setConfiguration();

private:
    Ui::ConfigureGeneral *ui;
};

#endif // CONFIGURE_GENERAL_H
