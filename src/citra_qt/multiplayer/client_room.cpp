// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <future>
#include <QColor>
#include <QImage>
#include <QList>
#include <QLocale>
#include <QMetaType>
#include <QTime>
#include <QtConcurrent/QtConcurrentRun>
#include "citra_qt/game_list_p.h"
#include "citra_qt/multiplayer/client_room.h"
#include "citra_qt/multiplayer/message.h"
#include "common/logging/log.h"
#include "core/announce_multiplayer_session.h"
#include "ui_client_room.h"

ClientRoomWindow::ClientRoomWindow(QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(std::make_unique<Ui::ClientRoom>()) {
    ui->setupUi(this);

    // setup the callbacks for network updates
    if (auto member = Network::GetRoomMember().lock()) {
        member->BindOnRoomInformationChanged(
            [this](const Network::RoomInformation& info) { emit RoomInformationChanged(info); });
        member->BindOnStateChanged(
            [this](const Network::RoomMember::State& state) { emit StateChanged(state); });

        connect(this, &ClientRoomWindow::RoomInformationChanged, this,
                &ClientRoomWindow::OnRoomUpdate);
        connect(this, &ClientRoomWindow::StateChanged, this, &::ClientRoomWindow::OnStateChange);
    } else {
        // TODO (jroweboy) network was not initialized?
    }

    connect(ui->disconnect, &QPushButton::pressed, [this] { Disconnect(); });
    ui->disconnect->setDefault(false);
    ui->disconnect->setAutoDefault(false);
    UpdateView();
}

void ClientRoomWindow::OnRoomUpdate(const Network::RoomInformation& info) {
    UpdateView();
}

void ClientRoomWindow::OnStateChange(const Network::RoomMember::State& state) {
    switch (state) {
    case Network::RoomMember::State::Idle:
        NGLOG_INFO(Network, "State: Idle");
        break;
    case Network::RoomMember::State::Joining:
        NGLOG_INFO(Network, "State: Joining");
        break;
    case Network::RoomMember::State::Joined:
        NGLOG_INFO(Network, "State: Joined");
        ui->chat->Clear();
        ui->chat->AppendStatusMessage(tr("Connected"));
        break;
    case Network::RoomMember::State::LostConnection:
        NetworkMessage::ShowError(NetworkMessage::LOST_CONNECTION);
        NGLOG_INFO(Network, "State: LostConnection");
        break;
    case Network::RoomMember::State::CouldNotConnect:
        NetworkMessage::ShowError(NetworkMessage::UNABLE_TO_CONNECT);
        NGLOG_INFO(Network, "State: CouldNotConnect");
        break;
    case Network::RoomMember::State::NameCollision:
        NetworkMessage::ShowError(NetworkMessage::USERNAME_IN_USE);
        NGLOG_INFO(Network, "State: NameCollision");
        break;
    case Network::RoomMember::State::MacCollision:
        NetworkMessage::ShowError(NetworkMessage::MAC_COLLISION);
        NGLOG_INFO(Network, "State: MacCollision");
        break;
    case Network::RoomMember::State::WrongPassword:
        NetworkMessage::ShowError(NetworkMessage::WRONG_PASSWORD);
        NGLOG_INFO(Network, "State: WrongPassword");
        break;
    case Network::RoomMember::State::WrongVersion:
        NetworkMessage::ShowError(NetworkMessage::WRONG_VERSION);
        NGLOG_INFO(Network, "State: WrongVersion");
        break;
    default:
        break;
    }
    UpdateView();
}

void ClientRoomWindow::Disconnect() {
    if (!NetworkMessage::WarnDisconnect()) {
        return;
    }
    if (auto member = Network::GetRoomMember().lock()) {
        member->Leave();
        ui->chat->AppendStatusMessage(tr("Disconnected"));
        close();
        emit Closed();
    }
}

void ClientRoomWindow::UpdateView() {
    if (auto member = Network::GetRoomMember().lock()) {
        if (member->IsConnected()) {
            ui->chat->Enable();
            ui->disconnect->setEnabled(true);
            auto memberlist = member->GetMemberInformation();
            ui->chat->SetPlayerList(memberlist);
            const auto information = member->GetRoomInformation();
            setWindowTitle(QString(tr("%1 (%2/%3 members) - connected"))
                               .arg(QString::fromStdString(information.name))
                               .arg(memberlist.size())
                               .arg(information.member_slots));
            return;
        }
    }
    // TODO(B3N30): can't get RoomMember*, show error and close window
    close();
    emit Closed();
    return;
}
