// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <unordered_map>
#include "input_common/main.h"
#include "input_common/udp/client.h"

namespace InputCommon::CemuhookUDP {

class UDPTouchDevice;
class UDPMotionDevice;

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
