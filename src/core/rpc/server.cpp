#include <functional>

#include "common/threadsafe_queue.h"
#include "core/core.h"
#include "core/rpc/rpc_server.h"
#include "core/rpc/server.h"

namespace RPC {

Server::Server(RPCServer& rpc_server) : rpc_server(rpc_server) {}

void Server::Start() {
    const auto callback = [this](std::unique_ptr<RPC::Packet> new_request) {
        NewRequestCallback(std::move(new_request));
    };

    try {
        zmq_server = std::make_unique<ZMQServer>(callback);
    } catch (...) {
        LOG_ERROR(RPC_Server, "Error starting ZeroMQ server");
    }
}

void Server::Stop() {
    zmq_server.reset();
}

void Server::NewRequestCallback(std::unique_ptr<RPC::Packet> new_request) {
    LOG_INFO(RPC_Server, "Received request version={} id={} type={} size={}",
             new_request->GetVersion(), new_request->GetId(),
             static_cast<u32>(new_request->GetPacketType()), new_request->GetPacketDataSize());
    rpc_server.QueueRequest(std::move(new_request));
}

}; // namespace RPC
