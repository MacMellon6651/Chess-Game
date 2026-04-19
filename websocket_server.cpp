#include "websocket_server.hpp"
#include "server.hpp"
#include <iostream>
#include <boost/beast/http.hpp>

namespace beast = boost::beast;
namespace http = beast::http; 

WebSocketSession::WebSocketSession(tcp::socket socket, ChessServer& server)
    : _ws(std::move(socket))
    , _server(server) {
    
    // Настройка WebSocket
    _ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
    _ws.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "ChessGame WebSocket Server");
        }));
}

WebSocketSession::~WebSocketSession() {
    close();
}

void WebSocketSession::start() {
    BOOST_LOG_TRIVIAL(info) << "New WebSocket session started!";
    
    // Асинхронное принятие WebSocket соединения
    _ws.async_accept(
        beast::bind_front_handler(
            [self = shared_from_this()](beast::error_code ec) {
                if (!ec) {
                    self->do_read();
                } else {
                    BOOST_LOG_TRIVIAL(error) << "WebSocket accept error: " << ec.message();
                }
            }));
}

void WebSocketSession::send(const nlohmann::json& msg) {
    // Преобразуем в строку и отправляем
    auto payload = Protocol::serialize_json(msg);
    auto self = shared_from_this();
    
    boost::asio::post(_ws.get_executor(), [this, self, payload = std::move(payload)]() {
        bool writing = !_outbox.empty();
        _outbox.push_back(std::move(payload));
        if (!writing) {
            do_write();
        }
    });
}

void WebSocketSession::close() {
    boost::system::error_code ec;
    _ws.close(websocket::close_code::normal, ec);
}

void WebSocketSession::do_read() {
    auto self = shared_from_this();
    
    _ws.async_read(
        _buffer,
        [this, self](beast::error_code ec, std::size_t bytes_transferred) {
            static_cast<void>(bytes_transferred);
            
            if (ec) {
                BOOST_LOG_TRIVIAL(info) << "WebSocket read error: " << ec.message();
                on_disconnect();
                return;
            }
            
            std::string data = beast::buffers_to_string(_buffer.data());
            _buffer.consume(_buffer.size());
            
            BOOST_LOG_TRIVIAL(debug) << "WebSocket received: " << data;
            handle_command(data);
            
            do_read();
        });
}

void WebSocketSession::do_write() {
    auto self = shared_from_this();
    
    _ws.async_write(
        net::buffer(_outbox.front()),
        [this, self](beast::error_code ec, std::size_t bytes_transferred) {
            static_cast<void>(bytes_transferred);
            
            if (ec) {
                BOOST_LOG_TRIVIAL(error) << "WebSocket write error: " << ec.message();
                on_disconnect();
                return;
            }
            
            _outbox.pop_front();
            if (!_outbox.empty()) {
                do_write();
            }
        });
}

void WebSocketSession::handle_command(const std::string& data) {
    GameCommand cmd = Protocol::parse(data);
    
    if (!cmd.is_valid) {
        send({{"status", "error"}, {"message", "Invalid JSON format"}});
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "WebSocket command: " << cmd.type;
    
    if (cmd.type == "ping") {
        send({{"status", "ok"}, {"message", "pong"}});
        return;
    }
    
    if (cmd.type == "register") {
        // Обработка регистрации
        if (cmd.nick.empty()) {
            send({{"status", "error"}, {"message", "nick required"}});
            return;
        }
        
        if (_server.get_db()->register_user(cmd.nick, cmd.password)) {
            send({{"status", "ok"}, {"message", "registered"}});
            // После регистрации автоматически авторизуем
            cmd.type = "auth";
        } else {
            send({{"status", "error"}, {"message", "Registration failed"}});
            return;
        }
    }
    
    if (cmd.type == "auth") {
        _server.handle_websocket_auth(shared_from_this(), cmd);
        return;
    }
    
    if (!_authorized) {
        send({{"status", "error"}, {"message", "Not authorized"}});
        return;
    }
    
    if (cmd.type == "queue") {
        _server.handle_websocket_queue(shared_from_this());
        return;
    }
    
    if (cmd.type == "leave") {
        _server.handle_websocket_leave(shared_from_this());
        return;
    }
    
    if (cmd.type == "chat") {
        _server.handle_websocket_chat(shared_from_this(), cmd);
        return;
    }
    
    if (cmd.type == "move") {
        _server.handle_websocket_move(shared_from_this(), cmd);
        return;
    }
    
    if (cmd.type == "draw") {
        _server.handle_websocket_draw(shared_from_this(), cmd);
        return;
    }
    
    send({{"status", "error"}, {"message", "Unknown command type: " + cmd.type}});
}

void WebSocketSession::on_disconnect() {
    if (_authorized) {
        _server.on_websocket_disconnect(shared_from_this());
    }
}

WebSocketServer::WebSocketServer(net::io_context& io, const ServerSettings& config, ChessServer& server)
    : _io(io)
    , _acceptor(io, tcp::endpoint(boost::asio::ip::make_address(config.host), config.port + 1))
    , _server(server) {
    
    BOOST_LOG_TRIVIAL(info) << "WebSocket Server initialized on " << config.host << ":" << (config.port + 1);
}

void WebSocketServer::start() {
    do_accept();
}

void WebSocketServer::stop() {
    _running = false;
    _acceptor.close();
}

void WebSocketServer::do_accept() {
    if (!_running) return;
    
    _acceptor.async_accept(
        [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                self->on_accept(ec, std::move(socket));
            }
            self->do_accept();
        });
}

void WebSocketServer::on_accept(beast::error_code ec, tcp::socket socket) {
    if (ec) {
        BOOST_LOG_TRIVIAL(error) << "WebSocket accept error: " << ec.message();
        return;
    }
    
    auto session = std::make_shared<WebSocketSession>(std::move(socket), _server);
    _sessions.push_back(session);
    session->start();
}

void WebSocketServer::broadcast(const nlohmann::json& msg) {
    for (auto& session : _sessions) {
        if (session && session->is_authorized()) {
            session->send(msg);
        }
    }
}