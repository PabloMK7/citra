// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "citra_qt/multiplayer/chat_room.h"
#include "ui_client_room.h"

class ClientRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit ClientRoomWindow(QWidget* parent);

public slots:
    void OnRoomUpdate(const Network::RoomInformation&);
    void OnStateChange(const Network::RoomMember::State&);

signals:
    /**
     * Signalled by this widget when it is closing itself and destroying any state such as
     * connections that it might have.
     */
    void Closed();
    void RoomInformationChanged(const Network::RoomInformation&);
    void StateChanged(const Network::RoomMember::State&);

private:
    void Disconnect();
    void UpdateView();

    QStandardItemModel* player_list;
    std::unique_ptr<Ui::ClientRoom> ui;
};
