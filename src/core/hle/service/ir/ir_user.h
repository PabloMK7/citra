// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include "core/hle/service/service.h"

namespace Service {
namespace IR {

/// An interface representing a device that can communicate with 3DS via ir:USER service
class IRDevice {
public:
    /**
     * A function object that implements the method to send data to the 3DS, which takes a vector of
     * data to send.
     */
    using SendFunc = std::function<void(const std::vector<u8>& data)>;

    explicit IRDevice(SendFunc send_func);
    virtual ~IRDevice();

    /// Called when connected with 3DS
    virtual void OnConnect() = 0;

    /// Called when disconnected from 3DS
    virtual void OnDisconnect() = 0;

    /// Called when data is received from the 3DS. This is invoked by the ir:USER send function.
    virtual void OnReceive(const std::vector<u8>& data) = 0;

protected:
    /// Sends data to the 3DS. The actual sending method is specified in the constructor
    void Send(const std::vector<u8>& data);

private:
    const SendFunc send_func;
};

class IR_User_Interface : public Service::Interface {
public:
    IR_User_Interface();

    std::string GetPortName() const override {
        return "ir:USER";
    }
};

void InitUser();
void ShutdownUser();

/// Reload input devices. Used when input configuration changed
void ReloadInputDevicesUser();

} // namespace IR
} // namespace Service
