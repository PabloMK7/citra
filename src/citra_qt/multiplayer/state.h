// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QWidget>
#include "network/network.h"

class QStandardItemModel;
class Lobby;
class HostRoomWindow;
class ClientRoomWindow;
class DirectConnectWindow;
class ClickableLabel;
namespace Core {
class AnnounceMultiplayerSession;
}

class MultiplayerState : public QWidget {
    Q_OBJECT;

public:
    explicit MultiplayerState(QWidget* parent, QStandardItemModel* game_list, QAction* leave_room,
                              QAction* show_room);
    ~MultiplayerState();

    /**
     * Close all open multiplayer related dialogs
     */
    void Close();

    ClickableLabel* GetStatusText() const {
        return status_text;
    }

    ClickableLabel* GetStatusIcon() const {
        return status_icon;
    }

public slots:
    void OnNetworkStateChanged(const Network::RoomMember::State& state);
    void OnViewLobby();
    void OnCreateRoom();
    bool OnCloseRoom();
    void OnOpenNetworkRoom();
    void OnDirectConnectToRoom();
    void OnAnnounceFailed(const Common::WebResult&);

signals:
    void NetworkStateChanged(const Network::RoomMember::State&);
    void AnnounceFailed(const Common::WebResult&);

private:
    Lobby* lobby = nullptr;
    HostRoomWindow* host_room = nullptr;
    ClientRoomWindow* client_room = nullptr;
    DirectConnectWindow* direct_connect = nullptr;
    ClickableLabel* status_icon = nullptr;
    ClickableLabel* status_text = nullptr;
    QStandardItemModel* game_list_model = nullptr;
    QAction* leave_room;
    QAction* show_room;
    std::shared_ptr<Core::AnnounceMultiplayerSession> announce_multiplayer_session;
    Network::RoomMember::CallbackHandle<Network::RoomMember::State> state_callback_handle;
};

Q_DECLARE_METATYPE(Common::WebResult);
