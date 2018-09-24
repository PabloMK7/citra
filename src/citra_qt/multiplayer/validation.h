// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QRegExp>
#include <QValidator>

class Validation {
public:
    Validation()
        : room_name(room_name_regex), nickname(nickname_regex), ip(ip_regex), port(0, 65535) {}

    ~Validation() = default;

    const QValidator* GetRoomName() const {
        return &room_name;
    }
    const QValidator* GetNickname() const {
        return &nickname;
    }
    const QValidator* GetIP() const {
        return &ip;
    }
    const QValidator* GetPort() const {
        return &port;
    }

private:
    /// room name can be alphanumeric and " " "_" "." and "-" and must have a size of 4-20
    QRegExp room_name_regex = QRegExp("^[a-zA-Z0-9._- ]{4,20}$");
    QRegExpValidator room_name;

    /// nickname can be alphanumeric and " " "_" "." and "-" and must have a size of 4-20
    QRegExp nickname_regex = QRegExp("^[a-zA-Z0-9._- ]{4,20}$");
    QRegExpValidator nickname;

    /// ipv4 address only
    // TODO remove this when we support hostnames in direct connect
    QRegExp ip_regex = QRegExp(
        "(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|"
        "2[0-4][0-9]|25[0-5])");
    QRegExpValidator ip;

    /// port must be between 0 and 65535
    QIntValidator port;
};
