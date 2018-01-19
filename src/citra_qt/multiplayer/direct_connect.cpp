// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QFuture>
#include <QIntValidator>
#include <QRegExpValidator>
#include <QString>
#include <QtConcurrent/QtConcurrentRun>
#include "citra_qt/main.h"
#include "citra_qt/multiplayer/client_room.h"
#include "citra_qt/multiplayer/direct_connect.h"
#include "citra_qt/multiplayer/message.h"
#include "citra_qt/multiplayer/validation.h"
#include "citra_qt/ui_settings.h"
#include "core/settings.h"
#include "network/network.h"
#include "ui_direct_connect.h"

enum class ConnectionType : u8 { TRAVERSAL_SERVER, IP };

DirectConnectWindow::DirectConnectWindow(QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(new Ui::DirectConnect) {

    ui->setupUi(this);

    // setup the watcher for background connections
    watcher = new QFutureWatcher<void>;
    connect(watcher, &QFutureWatcher<void>::finished, this, &DirectConnectWindow::OnConnection);

    ui->nickname->setValidator(Validation::nickname);
    ui->nickname->setText(UISettings::values.nickname);
    ui->ip->setValidator(Validation::ip);
    ui->ip->setText(UISettings::values.ip);
    ui->port->setValidator(Validation::port);
    ui->port->setText(UISettings::values.port);

    // TODO(jroweboy): Show or hide the connection options based on the current value of the combo
    // box. Add this back in when the traversal server support is added.
    connect(ui->connect, &QPushButton::pressed, this, &DirectConnectWindow::Connect);
}

void DirectConnectWindow::Connect() {
    ClearAllError();
    bool isValid = true;
    if (!ui->nickname->hasAcceptableInput()) {
        isValid = false;
        ShowError(NetworkMessage::USERNAME_NOT_VALID);
    }
    if (const auto member = Network::GetRoomMember().lock()) {
        if (member->IsConnected()) {
            if (!NetworkMessage::WarnDisconnect()) {
                return;
            }
        }
    }
    switch (static_cast<ConnectionType>(ui->connection_type->currentIndex())) {
    case ConnectionType::TRAVERSAL_SERVER:
        break;
    case ConnectionType::IP:
        if (!ui->ip->hasAcceptableInput()) {
            isValid = false;
            NetworkMessage::ShowError(NetworkMessage::IP_ADDRESS_NOT_VALID);
        }
        if (!ui->port->hasAcceptableInput()) {
            isValid = false;
            NetworkMessage::ShowError(NetworkMessage::PORT_NOT_VALID);
        }
        break;
    }

    if (!isValid) {
        return;
    }

    // Store settings
    UISettings::values.nickname = ui->nickname->text();
    UISettings::values.ip = ui->ip->text();
    UISettings::values.port = (ui->port->isModified() && !ui->port->text().isEmpty())
                                  ? ui->port->text()
                                  : UISettings::values.port;
    Settings::Apply();

    // attempt to connect in a different thread
    QFuture<void> f = QtConcurrent::run([&] {
        if (auto room_member = Network::GetRoomMember().lock()) {
            auto port = UISettings::values.port.toUInt();
            room_member->Join(ui->nickname->text().toStdString(),
                              ui->ip->text().toStdString().c_str(), port, 0,
                              Network::NoPreferredMac, ui->password->text().toStdString().c_str());
        }
    });
    watcher->setFuture(f);
    // and disable widgets and display a connecting while we wait
    BeginConnecting();
}

void DirectConnectWindow::ClearAllError() {}

void DirectConnectWindow::BeginConnecting() {
    ui->connect->setEnabled(false);
    ui->connect->setText(tr("Connecting"));
}

void DirectConnectWindow::EndConnecting() {
    ui->connect->setEnabled(true);
    ui->connect->setText(tr("Connect"));
}

void DirectConnectWindow::OnConnection() {
    EndConnecting();

    bool isConnected = true;
    if (auto room_member = Network::GetRoomMember().lock()) {
        switch (room_member->GetState()) {
        case Network::RoomMember::State::CouldNotConnect:
            isConnected = false;
            ShowError(NetworkMessage::UNABLE_TO_CONNECT);
            break;
        case Network::RoomMember::State::NameCollision:
            isConnected = false;
            ShowError(NetworkMessage::USERNAME_IN_USE);
            break;
        case Network::RoomMember::State::Joining:
            auto parent = static_cast<GMainWindow*>(parentWidget());
            parent->OnOpenNetworkRoom();
            close();
        }
    }
}
