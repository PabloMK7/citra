#include "common/common_types.h"
#include "core/core.h"
#include "core/rpc/packet.h"
#include "core/rpc/zmq_server.h"

namespace RPC {

ZMQServer::ZMQServer(std::function<void(std::unique_ptr<Packet>)> new_request_callback)
    : zmq_context(std::move(std::make_unique<zmq::context_t>(1))),
      zmq_socket(std::move(std::make_unique<zmq::socket_t>(*zmq_context, ZMQ_REP))),
      new_request_callback(std::move(new_request_callback)) {
    // Use a random high port
    // TODO: Make configurable or increment port number on failure
    zmq_socket->bind("tcp://127.0.0.1:45987");
    LOG_INFO(RPC_Server, "ZeroMQ listening on port 45987");

    worker_thread = std::thread(&ZMQServer::WorkerLoop, this);
}

ZMQServer::~ZMQServer() {
    // Triggering the zmq_context destructor will cancel
    // any blocking calls to zmq_socket->recv()
    running = false;
    zmq_context.reset();
    worker_thread.join();

    LOG_INFO(RPC_Server, "ZeroMQ stopped");
}

void ZMQServer::WorkerLoop() {
    zmq::message_t request;
    while (running) {
        try {
            if (zmq_socket->recv(&request, 0)) {
                if (request.size() >= MIN_PACKET_SIZE && request.size() <= MAX_PACKET_SIZE) {
                    u8* request_buffer = static_cast<u8*>(request.data());
                    PacketHeader header;
                    std::memcpy(&header, request_buffer, sizeof(header));
                    if ((request.size() - MIN_PACKET_SIZE) == header.packet_size) {
                        u8* data = request_buffer + MIN_PACKET_SIZE;
                        std::function<void(Packet&)> send_reply_callback =
                            std::bind(&ZMQServer::SendReply, this, std::placeholders::_1);
                        std::unique_ptr<Packet> new_packet =
                            std::make_unique<Packet>(header, data, send_reply_callback);

                        // Send the request to the upper layer for handling
                        new_request_callback(std::move(new_packet));
                    }
                }
            }
        } catch (...) {
            LOG_WARNING(RPC_Server, "Failed to receive data on ZeroMQ socket");
        }
    }
    std::unique_ptr<Packet> end_packet = nullptr;
    new_request_callback(std::move(end_packet));
    // Destroying the socket must be done by this thread.
    zmq_socket.reset();
}

void ZMQServer::SendReply(Packet& reply_packet) {
    if (running) {
        auto reply_buffer =
            std::make_unique<u8[]>(MIN_PACKET_SIZE + reply_packet.GetPacketDataSize());
        auto reply_header = reply_packet.GetHeader();

        std::memcpy(reply_buffer.get(), &reply_header, sizeof(reply_header));
        std::memcpy(reply_buffer.get() + (4 * sizeof(u32)), reply_packet.GetPacketData().data(),
                    reply_packet.GetPacketDataSize());

        zmq_socket->send(reply_buffer.get(), MIN_PACKET_SIZE + reply_packet.GetPacketDataSize());

        LOG_INFO(RPC_Server, "Sent reply version({}) id=({}) type=({}) size=({})",
                 reply_packet.GetVersion(), reply_packet.GetId(),
                 static_cast<u32>(reply_packet.GetPacketType()), reply_packet.GetPacketDataSize());
    }
}

}; // namespace RPC
