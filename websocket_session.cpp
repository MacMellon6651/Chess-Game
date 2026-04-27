#include "websocket_session.hpp"
#include "server.hpp"
#include "command.hpp"
#include "protocol.hpp"
#include <boost/beast/http.hpp>

WebSocketSession::WebSocketSession(tcp::socket socket, ChessServer& server)
    : _ws(std::move(socket)), _server(server) {
    _ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    _ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(beast::http::field::server, "ChessGame WebSocket Server");
        }));
}

WebSocketSession::~WebSocketSession() { close(); }

void WebSocketSession::start() {
    auto self = shared_from_this();
    _ws.async_accept([self](beast::error_code ec) {
        if (!ec) self->do_read();
        else self->on_disconnect();
    });
}

void WebSocketSession::send(const nlohmann::json& msg) {
    auto payload = ProtocolParser::serialize_json(msg);
    auto self = shared_from_this();
    net::post(_ws.get_executor(), [this, self, payload]() {
        bool writing = !_outbox.empty();
        _outbox.push_back(payload);
        if (!writing) do_write();
    });
}

void WebSocketSession::close() {
    beast::error_code ec;
    _ws.close(websocket::close_code::normal, ec);
}

void WebSocketSession::do_read() {
    auto self = shared_from_this();
    _ws.async_read(_buffer, [this, self](beast::error_code ec, std::size_t) {
        if (ec) {
            on_disconnect();
            return;
        }
        std::string data = beast::buffers_to_string(_buffer.data());
        _buffer.consume(_buffer.size());
        handle_command(data);
        do_read();
    });
}

void WebSocketSession::do_write() {
    auto self = shared_from_this();
    _ws.async_write(net::buffer(_outbox.front()),
        [this, self](beast::error_code ec, std::size_t) {
            if (ec) {
                on_disconnect();
                return;
            }
            _outbox.pop_front();
            if (!_outbox.empty()) do_write();
        });
}

void WebSocketSession::handle_command(const std::string& data) {
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

void WebSocketSession::on_disconnect() {
    _server.on_disconnect(shared_from_this());
}