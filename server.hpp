#pragma once

#include "player_interface.hpp"
#include "game_room.hpp"
#include "config.hpp"
#include "database.hpp"
#include "command.hpp"   // добавить
#include <boost/asio.hpp>
#include <memory>
#include <deque>
#include <vector>

using boost::asio::ip::tcp;   // добавить

class ChessServer {
public:
    ChessServer(boost::asio::io_context& io, const ServerSettings& config);
    ~ChessServer();

    void start_accept_tcp();
    void start_websocket_server();

    void handle_auth(std::shared_ptr<IPlayer> player, const GameCommand& cmd);
    void handle_queue(std::shared_ptr<IPlayer> player);
    void handle_leave(std::shared_ptr<IPlayer> player);
    void handle_chat(std::shared_ptr<IPlayer> player, const GameCommand& cmd);
    void handle_move(std::shared_ptr<IPlayer> player, const GameCommand& cmd);
    void handle_draw(std::shared_ptr<IPlayer> player, const GameCommand& cmd);
    void handle_leaderboard(std::shared_ptr<IPlayer> player);

    void on_disconnect(std::shared_ptr<IPlayer> player);

    std::shared_ptr<Database> db() { return _db; }
    boost::asio::io_context& io_context() { return _io; }

private:
    void try_matchmake();
    void remove_from_queue(std::shared_ptr<IPlayer> player);
    void broadcast_queue_size();
    void accept_websocket();

    boost::asio::io_context& _io;
    tcp::acceptor _tcp_acceptor;
    std::unique_ptr<tcp::acceptor> _ws_acceptor;

    std::vector<std::shared_ptr<IPlayer>> _all_players;
    std::deque<std::shared_ptr<IPlayer>> _queue;
    std::vector<std::shared_ptr<GameRoom>> _rooms;
    std::shared_ptr<Database> _db;
};