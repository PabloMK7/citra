// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QRegExp>
#include <QValidator>

class Validation {
public:
    static Validation get() {
        static Validation validation;
        return validation;
    }

    ~Validation() {
        delete room_name;
        delete nickname;
        delete ip;
        delete port;
    }

    /// room name can be alphanumeric and " " "_" "." and "-"
    QRegExp room_name_regex = QRegExp("^[a-zA-Z0-9._- ]+$");
    const QValidator* room_name = new QRegExpValidator(room_name_regex);

    /// nickname can be alphanumeric and " " "_" "." and "-"
    QRegExp nickname_regex = QRegExp("^[a-zA-Z0-9._- ]+$");
    const QValidator* nickname = new QRegExpValidator(nickname_regex);

    /// ipv4 address only
    // TODO remove this when we support hostnames in direct connect
    QRegExp ip_regex = QRegExp(
        "(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|"
        "2[0-4][0-9]|25[0-5])");
    const QValidator* ip = new QRegExpValidator(ip_regex);

    /// port must be between 0 and 65535
    const QValidator* port = new QIntValidator(0, 65535);

private:
    Validation() = default;
};
