// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace FS_User

namespace FS_User {

class FS_Path {
public:
    // Command to access archive file
    enum LowPathType : u32 {
        Invalid = 0,
        Empty   = 1,
        Binary  = 2,
        Char    = 3,
        Wchar   = 4
    };

    FS_Path(LowPathType type, u32 size, u32 pointer);

    LowPathType GetType() const;

    const std::vector<u8>& GetBinary() const;
    const std::string& GetString() const;
    const std::u16string& GetU16Str() const;

    std::string AsString();
    std::u16string AsU16Str();

private:
    LowPathType type;
    std::vector<u8> binary;
    std::string string;
    std::u16string u16str;
};

/// Interface to "fs:USER" service
class Interface : public Service::Interface {
public:

    Interface();

    ~Interface();

    /**
     * Gets the string port name used by CTROS for the service
     * @return Port name of service
     */
    std::string GetPortName() const override {
        return "fs:USER";
    }
};

} // namespace
