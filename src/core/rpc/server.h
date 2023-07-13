// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Core::RPC {

class RPCServer;
class UDPServer;
class Packet;

class Server {
public:
    explicit Server(RPCServer& rpc_server);
    ~Server();

    void NewRequestCallback(std::unique_ptr<Packet> new_request);

private:
    RPCServer& rpc_server;
    std::unique_ptr<UDPServer> udp_server;
};

} // namespace Core::RPC
