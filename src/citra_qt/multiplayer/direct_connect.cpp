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
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/multiplayer/validation.h"
#include "citra_qt/ui_settings.h"
#include "core/settings.h"
#include "network/network.h"
#include "ui_direct_connect.h"

enum class ConnectionType : u8 { TraversalServer, IP };

DirectConnectWindow::DirectConnectWindow(QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::DirectConnect>()) {

    ui->setupUi(this);

    // setup the watcher for background connections
    watcher = new QFutureWatcher<void>;
    connect(watcher, &QFutureWatcher<void>::finished, this, &DirectConnectWindow::OnConnection);

    ui->nickname->setValidator(validation.GetNickname());
    ui->nickname->setText(UISettings::values.nickname);
    if (ui->nickname->text().isEmpty() && !Settings::values.citra_username.empty()) {
        // Use Citra Web Service user name as nickname by default
        ui->nickname->setText(QString::fromStdString(Settings::values.citra_username));
    }
    ui->ip->setValidator(validation.GetIP());
    ui->ip->setText(UISettings::values.ip);
    ui->port->setValidator(validation.GetPort());
    ui->port->setText(UISettings::values.port);

    // TODO(jroweboy): Show or hide the connection options based on the current value of the combo
    // box. Add this back in when the traversal server support is added.
    connect(ui->connect, &QPushButton::pressed, this, &DirectConnectWindow::Connect);
}

DirectConnectWindow::~DirectConnectWindow() = default;

void DirectConnectWindow::RetranslateUi() {
    ui->retranslateUi(this);
}

void DirectConnectWindow::Connect() {
    if (!ui->nickname->hasAcceptableInput()) {
        NetworkMessage::ShowError(NetworkMessage::USERNAME_NOT_VALID);
        return;
    }
    if (const auto member = Network::GetRoomMember().lock()) {
        // Prevent the user from trying to join a room while they are already joining.
        if (member->GetState() == Network::RoomMember::State::Joining) {
            return;
        } else if (member->GetState() == Network::RoomMember::State::Joined) {
            // And ask if they want to leave the room if they are already in one.
            if (!NetworkMessage::WarnDisconnect()) {
                return;
            }
        }
    }
    switch (static_cast<ConnectionType>(ui->connection_type->currentIndex())) {
    case ConnectionType::TraversalServer:
        break;
    case ConnectionType::IP:
        if (!ui->ip->hasAcceptableInput()) {
            NetworkMessage::ShowError(NetworkMessage::IP_ADDRESS_NOT_VALID);
            return;
        }
        if (!ui->port->hasAcceptableInput()) {
            NetworkMessage::ShowError(NetworkMessage::PORT_NOT_VALID);
            return;
        }
        break;
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

    if (auto room_member = Network::GetRoomMember().lock()) {
        if (room_member->GetState() == Network::RoomMember::State::Joined) {
            close();
        }
    }
}
