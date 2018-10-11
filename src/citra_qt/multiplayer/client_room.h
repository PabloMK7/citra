// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "citra_qt/multiplayer/chat_room.h"

namespace Ui {
class ClientRoom;
}

class ClientRoomWindow : public QDialog {
    Q_OBJECT

public:
    explicit ClientRoomWindow(QWidget* parent);
    ~ClientRoomWindow();

    void RetranslateUi();

public slots:
    void OnRoomUpdate(const Network::RoomInformation&);
    void OnStateChange(const Network::RoomMember::State&);

signals:
    void RoomInformationChanged(const Network::RoomInformation&);
    void StateChanged(const Network::RoomMember::State&);

private:
    void Disconnect();
    void UpdateView();

    QStandardItemModel* player_list;
    std::unique_ptr<Ui::ClientRoom> ui;
};
