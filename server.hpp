#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <deque>
#include <vector>
#include "config.hpp"
#include "command.hpp"
#include "logging.hpp"
#include "chess_engine/position.hpp"
#include "websocket_server.hpp"
#include "database.hpp"

using boost::asio::ip::tcp;

class ChessServer;
class GameRoom;

// Вспомогательные функции для преобразования координат
uint8_t algebraic_to_index(const std::string& sq);
std::string index_to_algebraic(uint8_t idx);

class Session : public std::enable_shared_from_this<Session> {
public:
    explicit Session(tcp::socket socket, ChessServer& server);
    tcp::socket& socket();
    void start();
    void send(const nlohmann::json& msg);
    void close();

    bool is_authorized() const;
    const std::string& nick() const;
    int rating() const;

private:
    void do_read();
    void do_write();
    void handle_command(const std::string& raw, const GameCommand& cmd);

    tcp::socket _socket;
    boost::asio::streambuf _buffer;
    boost::asio::strand<boost::asio::any_io_executor> _strand;
    ChessServer& _server;

    std::deque<std::string> _outbox;

    bool _authorized{false};
    std::string _nick;
    int _rating{1000};
    int _user_id{-1};
    std::weak_ptr<GameRoom> _room;

    friend class ChessServer;
    friend class GameRoom;
};

// Класс игровой комнаты с использованием шахматного движка
// server.hpp - найдите класс GameRoom и замените весь класс
class GameRoom : public std::enable_shared_from_this<GameRoom> {
public:
    // Один конструктор с server
    GameRoom(std::shared_ptr<Session> a, std::shared_ptr<Session> b, ChessServer& server);

    void start();
    void on_chat(const std::shared_ptr<Session>& from, const std::string& text);
    bool on_move(const std::shared_ptr<Session>& from, const std::string& from_sq, 
                 const std::string& to_sq, const std::string& promotion = "");
    void on_draw_request(const std::shared_ptr<Session>& from);
    bool on_draw_accept(const std::shared_ptr<Session>& from);
    bool on_draw_decline(const std::shared_ptr<Session>& from);
    void on_disconnect(const std::shared_ptr<Session>& who);

    bool has(const std::shared_ptr<Session>& s) const;
    std::shared_ptr<Session> opponent_of(const std::shared_ptr<Session>& s) const;
    std::string get_position_fen() const;

private:
    void broadcast(const nlohmann::json& msg);
    void end_game(const std::string& result, const std::shared_ptr<Session>& winner = nullptr);
    void update_rating(const std::shared_ptr<Session>& winner, const std::shared_ptr<Session>& loser);

    std::shared_ptr<Session> _a;
    std::shared_ptr<Session> _b;
    bool _draw_offer_a{false};
    bool _draw_offer_b{false};
    Position _position;
    ChessServer& _server;  // Ссылка на сервер
};

class ChessServer {
public:
    ChessServer(boost::asio::io_context& io, const ServerSettings& config);

    void handle_websocket_auth(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd);
    void handle_websocket_queue(const std::shared_ptr<WebSocketSession>& ws);
    void handle_websocket_leave(const std::shared_ptr<WebSocketSession>& ws);
    void handle_websocket_chat(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd);
    void handle_websocket_move(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd);
    void handle_websocket_draw(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd);
    void on_websocket_disconnect(const std::shared_ptr<WebSocketSession>& ws);

    std::shared_ptr<Session> get_or_create_session(const std::shared_ptr<WebSocketSession>& ws);
    std::shared_ptr<Database> get_db() { return db_; }

private:
    void start_accept();
    void on_disconnect(const std::shared_ptr<Session>& s);

    void handle_auth(const std::shared_ptr<Session>& s, const GameCommand& cmd);
    void handle_queue(const std::shared_ptr<Session>& s);
    void handle_leave(const std::shared_ptr<Session>& s);
    void handle_chat(const std::shared_ptr<Session>& s, const GameCommand& cmd);
    void handle_move(const std::shared_ptr<Session>& s, const GameCommand& cmd);
    void handle_draw(const std::shared_ptr<Session>& s, const GameCommand& cmd);

    void try_matchmake();
    void remove_from_queue(const std::shared_ptr<Session>& s);



    

    tcp::acceptor _acceptor;
    std::vector<std::shared_ptr<Session>> _sessions;
    std::vector<std::shared_ptr<GameRoom>> _rooms;
    std::deque<std::shared_ptr<Session>> _queue;
    std::vector<std::shared_ptr<WebSocketSession>> _web_sessions;
    std::unordered_map<std::string, std::shared_ptr<Session>> _nick_to_session;
    std::shared_ptr<Database> db_;

    friend class Session;
};