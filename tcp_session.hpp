#pragma once

#include "player_interface.hpp"
#include <boost/asio.hpp>
#include <deque>

using boost::asio::ip::tcp;

class ChessServer;

class TcpSession : public IPlayer, public std::enable_shared_from_this<TcpSession> {
public:
    TcpSession(tcp::socket socket, ChessServer& server);
    ~TcpSession();

    void start();
    void send(const nlohmann::json& msg) override;
    void close() override;

    bool is_authorized() const override { return _authorized; }
    void set_authorized(bool auth) override { _authorized = auth; }
    const std::string& nick() const override { return _nick; }
    void set_nick(const std::string& n) override { _nick = n; }
    int rating() const override { return _rating; }
    void set_rating(int r) override { _rating = r; }
    int user_id() const override { return _user_id; }
    void set_user_id(int id) override { _user_id = id; }

    std::weak_ptr<GameRoom> room() const override { return _room; }
    void set_room(std::weak_ptr<GameRoom> r) override { _room = r; }

    bool in_queue() const override { return _in_queue; }
    void set_in_queue(bool q) override { _in_queue = q; }

private:
    void do_read();
    void do_write();
    void handle_command(const std::string& data);

    tcp::socket _socket;
    boost::asio::streambuf _buffer;
    boost::asio::strand<boost::asio::any_io_executor> _strand;
    ChessServer& _server;
    std::deque<std::string> _outbox;

    bool _authorized = false;
    std::string _nick;
    int _rating = 1000;
    int _user_id = -1;
    std::weak_ptr<GameRoom> _room;
    bool _in_queue = false;
};