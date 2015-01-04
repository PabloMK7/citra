// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef _CONTROLLER_CONFIG_UTIL_HXX_
#define _CONTROLLER_CONFIG_UTIL_HXX_

#include <QWidget>
#include <QPushButton>

/* TODO(bunnei): ImplementMe

#include "config.h"

class GStickConfig : public QWidget
{
    Q_OBJECT

public:
    // change_receiver needs to have a OnKeyConfigChanged(common::Config::Control, int, const QString&) slot!
    GStickConfig(common::Config::Control leftid, common::Config::Control rightid, common::Config::Control upid, common::Config::Control downid, QObject* change_receiver, QWidget* parent = NULL);

signals:
    void LeftChanged();
    void RightChanged();
    void UpChanged();
    void DownChanged();

private:
    QPushButton* left;
    QPushButton* right;
    QPushButton* up;
    QPushButton* down;

    QPushButton* clear;
};

class GKeyConfigButton : public QPushButton
{
    Q_OBJECT

public:
    // TODO: change_receiver also needs to have an ActivePortChanged(const common::Config::ControllerPort&) signal
    // change_receiver needs to have a OnKeyConfigChanged(common::Config::Control, int, const QString&) slot!
    GKeyConfigButton(common::Config::Control id, const QIcon& icon, const QString& text, QObject* change_receiver, QWidget* parent);
    GKeyConfigButton(common::Config::Control id, const QString& text, QObject* change_receiver, QWidget* parent);

signals:
    void KeyAssigned(common::Config::Control id, int key, const QString& text);

private slots:
    void OnActivePortChanged(const common::Config::ControllerPort& config);

    void OnClicked();

    void keyPressEvent(QKeyEvent* event); // TODO: bGrabbed?
    void mousePressEvent(QMouseEvent* event);

private:
    common::Config::Control id;
    bool inputGrabbed;

    QString old_text;
};

class GButtonConfigGroup : public QWidget
{
    Q_OBJECT

public:
    // change_receiver needs to have a OnKeyConfigChanged(common::Config::Control, int, const QString&) slot!
    GButtonConfigGroup(const QString& name, common::Config::Control id, QObject* change_receiver, QWidget* parent = NULL);

private:
    GKeyConfigButton* config_button;

    common::Config::Control id;
};

*/

#endif  // _CONTROLLER_CONFIG_HXX_
