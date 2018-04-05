// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QIcon>
#include <QMessageBox>
#include <QStandardItemModel>
#include "citra_qt/game_list.h"
#include "citra_qt/multiplayer/client_room.h"
#include "citra_qt/multiplayer/direct_connect.h"
#include "citra_qt/multiplayer/host_room.h"
#include "citra_qt/multiplayer/lobby.h"
#include "citra_qt/multiplayer/message.h"
#include "citra_qt/multiplayer/state.h"
#include "citra_qt/util/clickable_label.h"
#include "common/announce_multiplayer_room.h"
#include "common/logging/log.h"

MultiplayerState::MultiplayerState(QWidget* parent, QStandardItemModel* game_list_model)
    : QWidget(parent), game_list_model(game_list_model) {
    if (auto member = Network::GetRoomMember().lock()) {
        // register the network structs to use in slots and signals
        state_callback_handle = member->BindOnStateChanged(
            [this](const Network::RoomMember::State& state) { emit NetworkStateChanged(state); });
        connect(this, &MultiplayerState::NetworkStateChanged, this,
                &MultiplayerState::OnNetworkStateChanged);
    }

    qRegisterMetaType<Network::RoomMember::State>();
    qRegisterMetaType<Common::WebResult>();
    announce_multiplayer_session = std::make_shared<Core::AnnounceMultiplayerSession>();
    announce_multiplayer_session->BindErrorCallback(
        [this](const Common::WebResult& result) { emit AnnounceFailed(result); });
    connect(this, &MultiplayerState::AnnounceFailed, this, &MultiplayerState::OnAnnounceFailed);

    status_text = new ClickableLabel(this);
    status_icon = new ClickableLabel(this);
    status_text->setToolTip(tr("Current connection status"));
    status_text->setText(tr("Not Connected. Click here to find a room!"));
    status_icon->setPixmap(QIcon::fromTheme("disconnected").pixmap(16));

    connect(status_text, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);
    connect(status_icon, &ClickableLabel::clicked, this, &MultiplayerState::OnOpenNetworkRoom);
}

MultiplayerState::~MultiplayerState() {
    if (state_callback_handle) {
        if (auto member = Network::GetRoomMember().lock()) {
            member->Unbind(state_callback_handle);
        }
    }
}

void MultiplayerState::Close() {
    if (host_room)
        host_room->close();
    if (direct_connect)
        direct_connect->close();
    if (client_room)
        client_room->close();
    if (lobby)
        lobby->close();
}

void MultiplayerState::OnNetworkStateChanged(const Network::RoomMember::State& state) {
    NGLOG_DEBUG(Frontend, "Network state change");
    if (state == Network::RoomMember::State::Joined) {
        status_icon->setPixmap(QIcon::fromTheme("connected").pixmap(16));
        status_text->setText(tr("Connected"));
        return;
    }
    status_icon->setPixmap(QIcon::fromTheme("disconnected").pixmap(16));
    status_text->setText(tr("Not Connected"));
}

void MultiplayerState::OnAnnounceFailed(const Common::WebResult& result) {
    announce_multiplayer_session->Stop();
    QMessageBox::warning(this, tr("Error"),
                         tr("Failed to announce the room to the public lobby.\nThe room will not "
                            "get listed publicly.\nError: ") +
                             QString::fromStdString(result.result_string),
                         QMessageBox::Ok);
}

static void BringWidgetToFront(QWidget* widget) {
    widget->show();
    widget->activateWindow();
    widget->raise();
}

void MultiplayerState::OnViewLobby() {
    if (lobby == nullptr) {
        lobby = new Lobby(this, game_list_model, announce_multiplayer_session);
    }
    BringWidgetToFront(lobby);
}

void MultiplayerState::OnCreateRoom() {
    if (host_room == nullptr) {
        host_room = new HostRoomWindow(this, game_list_model, announce_multiplayer_session);
    }
    BringWidgetToFront(host_room);
}

void MultiplayerState::OnCloseRoom() {
    if (auto room = Network::GetRoom().lock()) {
        if (room->GetState() == Network::Room::State::Open) {
            if (NetworkMessage::WarnCloseRoom()) {
                room->Destroy();
                announce_multiplayer_session->Stop();
            }
        }
    }
}

void MultiplayerState::OnOpenNetworkRoom() {
    if (auto member = Network::GetRoomMember().lock()) {
        if (member->IsConnected()) {
            if (client_room == nullptr) {
                client_room = new ClientRoomWindow(this);
            }
            BringWidgetToFront(client_room);
            return;
        }
    }
    // If the user is not a member of a room, show the lobby instead.
    // This is currently only used on the clickable label in the status bar
    OnViewLobby();
}

void MultiplayerState::OnDirectConnectToRoom() {
    if (direct_connect == nullptr) {
        direct_connect = new DirectConnectWindow(this);
    }
    BringWidgetToFront(direct_connect);
}
