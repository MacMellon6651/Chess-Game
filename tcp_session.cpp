#include "tcp_session.hpp"
#include "server.hpp"
#include "command.hpp"
#include "protocol.hpp"

TcpSession::TcpSession(tcp::socket socket, ChessServer& server)
    : _socket(std::move(socket))
    , _strand(_socket.get_executor())
    , _server(server) {}

TcpSession::~TcpSession() { close(); }

void TcpSession::start() {
    do_read();
}

void TcpSession::send(const nlohmann::json& msg) {
    auto payload = ProtocolParser::serialize_json(msg);
    auto self = shared_from_this();
    boost::asio::dispatch(_strand, [this, self, payload = std::move(payload)]() {
        bool writing = !_outbox.empty();
        _outbox.push_back(std::move(payload));
        if (!writing) do_write();
    });
}

void TcpSession::close() {
    boost::system::error_code ec;
    _socket.shutdown(tcp::socket::shutdown_both, ec);
    _socket.close(ec);
}

void TcpSession::do_read() {
    auto self = shared_from_this();
    boost::asio::async_read_until(_socket, _buffer, '\n',
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                std::string data;
                std::istream is(&_buffer);
                std::getline(is, data);
                handle_command(data);
                do_read();
            } else {
                _server.on_disconnect(shared_from_this());
            }
        });
}

void TcpSession::do_write() {
    auto self = shared_from_this();
    boost::asio::async_write(_socket, boost::asio::buffer(_outbox.front()),
        boost::asio::bind_executor(_strand,
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    _server.on_disconnect(shared_from_this());
                    return;
                }
                _outbox.pop_front();
                if (!_outbox.empty()) do_write();
            }));
}

void TcpSession::handle_command(const std::string& data) {
    GameCommand cmd = ProtocolParser::parse(data);
    if (!cmd.is_valid) {
        send(Protocol::StatusResponse::error("Invalid format").to_json());
        return;
    }

    if (cmd.type == "ping") {
        send(Protocol::StatusResponse::ok("pong").to_json());
        return;
    }
    if (cmd.type == "auth") {
        _server.handle_auth(shared_from_this(), cmd);
        return;
    }
    if (cmd.type == "leaderboard") {
        _server.handle_leaderboard(shared_from_this());
        return;
    }
    if (!_authorized) {
        send(Protocol::StatusResponse::error("Not authorized").to_json());
        return;
    }
    if (cmd.type == "queue") {
        _server.handle_queue(shared_from_this());
        return;
    }
    if (cmd.type == "leave") {
        _server.handle_leave(shared_from_this());
        return;
    }
    if (cmd.type == "chat") {
        _server.handle_chat(shared_from_this(), cmd);
        return;
    }
    if (cmd.type == "move") {
        _server.handle_move(shared_from_this(), cmd);
        return;
    }
    if (cmd.type == "draw") {
        _server.handle_draw(shared_from_this(), cmd);
        return;
    }
    send(Protocol::StatusResponse::error("Unknown command").to_json());
}