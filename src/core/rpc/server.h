// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace RPC {

class RPCServer;
class UDPServer;
class Packet;

class Server {
public:
    Server(RPCServer& rpc_server);
    ~Server();
    void Start();
    void Stop();
    void NewRequestCallback(std::unique_ptr<Packet> new_request);

private:
    RPCServer& rpc_server;
    std::unique_ptr<UDPServer> udp_server;
};

} // namespace RPC
