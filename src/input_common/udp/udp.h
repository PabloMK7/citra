// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace InputCommon::CemuhookUDP {

class Client;

class State {
public:
    State();
    ~State();
    void ReloadUDPClient();

private:
    std::unique_ptr<Client> client;
};

std::unique_ptr<State> Init();

} // namespace InputCommon::CemuhookUDP
