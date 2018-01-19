// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QRegExp>
#include <QValidator>

namespace Validation {
/// room name can be alphanumeric and " " "_" "." and "-"
static const QRegExp room_name_regex("^[a-zA-Z0-9._- ]+$");
static const QValidator* room_name = new QRegExpValidator(room_name_regex);

/// nickname can be alphanumeric and " " "_" "." and "-"
static const QRegExp nickname_regex("^[a-zA-Z0-9._- ]+$");
static const QValidator* nickname = new QRegExpValidator(nickname_regex);

/// ipv4 address only
// TODO remove this when we support hostnames in direct connect
static const QRegExp ip_regex(
    "(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|"
    "2[0-4][0-9]|25[0-5])");
static const QValidator* ip = new QRegExpValidator(ip_regex);

/// port must be between 0 and 65535
static const QValidator* port = new QIntValidator(0, 65535);
}; // namespace Validation
