// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/rpc/packet.h"
#include "core/rpc/zmq_server.h"

namespace RPC {

class RPCServer;
class ZMQServer;

class Server {
public:
    Server(RPCServer& rpc_server);
    void Start();
    void Stop();
    void NewRequestCallback(std::unique_ptr<RPC::Packet> new_request);

private:
    RPCServer& rpc_server;
    std::unique_ptr<ZMQServer> zmq_server;
};

} // namespace RPC
