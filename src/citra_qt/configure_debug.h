// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef CONFIGURE_DEBUG_H
#define CONFIGURE_DEBUG_H

#include <QWidget>

namespace Ui {
class ConfigureDebug;
}

class ConfigureDebug : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigureDebug(QWidget *parent = 0);
    ~ConfigureDebug();

    void applyConfiguration();

private:
    void setConfiguration();

private:
    Ui::ConfigureDebug *ui;
};

#endif // CONFIGURE_DEBUG_H
