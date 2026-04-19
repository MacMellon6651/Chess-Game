#pragma once

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <deque>
#include <vector>
#include <string>
#include "command.hpp"
#include "config.hpp" 

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// Forward declaration
class ChessServer;
class Session;
class GameRoom;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    explicit WebSocketSession(tcp::socket socket, ChessServer& server);
    ~WebSocketSession();

    void start();
    void send(const nlohmann::json& msg);
    void close();
    
    bool is_authorized() const { return _authorized; }
    const std::string& nick() const { return _nick; }
    int rating() const { return _rating; }
    int user_id() const { return _user_id; }

private:
    void do_read();
    void do_write();
    void handle_command(const std::string& data);
    void on_disconnect();

    websocket::stream<tcp::socket> _ws;
    ChessServer& _server;
    beast::flat_buffer _buffer;
    std::deque<std::string> _outbox;
    
    bool _authorized{false};
    std::string _nick;
    int _rating{1000};
    int _user_id{-1};
    std::weak_ptr<GameRoom> _room;
    
    friend class ChessServer;
    friend class GameRoom;
};

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
public:
    WebSocketServer(net::io_context& io, const ServerSettings& config, ChessServer& server);
    void start();
    void stop();
    
    void broadcast(const nlohmann::json& msg);

private:
    void do_accept();
    void on_accept(beast::error_code ec, tcp::socket socket);

    net::io_context& _io;
    tcp::acceptor _acceptor;
    ChessServer& _server;
    std::vector<std::shared_ptr<WebSocketSession>> _sessions;
    bool _running{true};
};