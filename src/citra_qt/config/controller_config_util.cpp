// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QPushButton>
#include <QStyle>
#include <QGridLayout>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QLabel>

#include "controller_config_util.h"

/* TODO(bunnei): ImplementMe
GStickConfig::GStickConfig(common::Config::Control leftid, common::Config::Control rightid, common::Config::Control upid, common::Config::Control downid, QObject* change_receiver, QWidget* parent) : QWidget(parent)
{
    left = new GKeyConfigButton(leftid, style()->standardIcon(QStyle::SP_ArrowLeft), QString(), change_receiver, this);
    right = new GKeyConfigButton(rightid, style()->standardIcon(QStyle::SP_ArrowRight), QString(), change_receiver, this);
    up = new GKeyConfigButton(upid, style()->standardIcon(QStyle::SP_ArrowUp), QString(), change_receiver, this);
    down = new GKeyConfigButton(downid, style()->standardIcon(QStyle::SP_ArrowDown), QString(), change_receiver, this);
    clear = new QPushButton(tr("Clear"), this);

    QGridLayout* layout = new QGridLayout(this);
    layout->addWidget(left, 1, 0);
    layout->addWidget(right, 1, 2);
    layout->addWidget(up, 0, 1);
    layout->addWidget(down, 2, 1);
    layout->addWidget(clear, 1, 1);

    setLayout(layout);
}

GKeyConfigButton::GKeyConfigButton(common::Config::Control id, const QIcon& icon, const QString& text, QObject* change_receiver, QWidget* parent) : QPushButton(icon, text, parent), id(id), inputGrabbed(false)
{
    connect(this, SIGNAL(clicked()), this, SLOT(OnClicked()));
    connect(this, SIGNAL(KeyAssigned(common::Config::Control, int, const QString&)), change_receiver, SLOT(OnKeyConfigChanged(common::Config::Control, int, const QString&)));
    connect(change_receiver, SIGNAL(ActivePortChanged(const common::Config::ControllerPort&)), this, SLOT(OnActivePortChanged(const common::Config::ControllerPort&)));
}

GKeyConfigButton::GKeyConfigButton(common::Config::Control id, const QString& text, QObject* change_receiver, QWidget* parent) : QPushButton(text, parent), id(id), inputGrabbed(false)
{
    connect(this, SIGNAL(clicked()), this, SLOT(OnClicked()));
    connect(this, SIGNAL(KeyAssigned(common::Config::Control, int, const QString&)), change_receiver, SLOT(OnKeyConfigChanged(common::Config::Control, int, const QString&)));
    connect(change_receiver, SIGNAL(ActivePortChanged(const common::Config::ControllerPort&)), this, SLOT(OnActivePortChanged(const common::Config::ControllerPort&)));
}

void GKeyConfigButton::OnActivePortChanged(const common::Config::ControllerPort& config)
{
    // TODO: Doesn't use joypad struct if that's the input source...
    QString text = QKeySequence(config.keys.key_code[id]).toString(); // has a nicer format
    if (config.keys.key_code[id] == Qt::Key_Shift) text = tr("Shift");
    else if (config.keys.key_code[id] == Qt::Key_Control) text = tr("Control");
    else if (config.keys.key_code[id] == Qt::Key_Alt) text = tr("Alt");
    else if (config.keys.key_code[id] == Qt::Key_Meta) text = tr("Meta");
    setText(text);
}

void GKeyConfigButton::OnClicked()
{
    grabKeyboard();
    grabMouse();
    inputGrabbed = true;

    old_text = text();
    setText(tr("Input..."));
}

void GKeyConfigButton::keyPressEvent(QKeyEvent* event)
{
    if (inputGrabbed)
    {
        releaseKeyboard();
        releaseMouse();
        setText(QString());

        // TODO: Doesn't capture "return" key
        // TODO: This doesn't quite work well, yet... find a better way
        QString text = QKeySequence(event->key()).toString(); // has a nicer format than event->text()
        int key = event->key();
        if (event->modifiers() == Qt::ShiftModifier) { text = tr("Shift"); key = Qt::Key_Shift; }
        else if (event->modifiers() == Qt::ControlModifier) { text = tr("Ctrl"); key = Qt::Key_Control; }
        else if (event->modifiers() == Qt::AltModifier) { text = tr("Alt"); key = Qt::Key_Alt; }
        else if (event->modifiers() == Qt::MetaModifier) { text = tr("Meta"); key = Qt::Key_Meta; }

        setText(old_text);
        emit KeyAssigned(id, key, text);

        inputGrabbed = false;

        // TODO: Keys like "return" cause another keyPressEvent to be generated after this one...
    }

    QPushButton::keyPressEvent(event); // TODO: Necessary?
}

void GKeyConfigButton::mousePressEvent(QMouseEvent* event)
{
    // Abort key assignment
    if (inputGrabbed)
    {
        releaseKeyboard();
        releaseMouse();
        setText(old_text);
        inputGrabbed = false;
    }

    QAbstractButton::mousePressEvent(event);
}

GButtonConfigGroup::GButtonConfigGroup(const QString& name, common::Config::Control id, QObject* change_receiver, QWidget* parent) : QWidget(parent), id(id)
{
    QHBoxLayout* layout = new QHBoxLayout(this);

    QPushButton* clear_button = new QPushButton(tr("Clear"));

    layout->addWidget(new QLabel(name, this));
    layout->addWidget(config_button = new GKeyConfigButton(id, QString(), change_receiver, this));
    layout->addWidget(clear_button);

    // TODO: connect config_button, clear_button

    setLayout(layout);
}

*/
