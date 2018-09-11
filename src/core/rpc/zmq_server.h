// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <thread>
#define ZMQ_STATIC
#include <zmq.hpp>

namespace RPC {

class Packet;

class ZMQServer {
public:
    explicit ZMQServer(std::function<void(std::unique_ptr<Packet>)> new_request_callback);
    ~ZMQServer();

private:
    void WorkerLoop();
    void SendReply(Packet& request);

    std::thread worker_thread;
    std::atomic_bool running = true;

    std::unique_ptr<zmq::context_t> zmq_context;
    std::unique_ptr<zmq::socket_t> zmq_socket;

    std::function<void(std::unique_ptr<Packet>)> new_request_callback;
};

} // namespace RPC
