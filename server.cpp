#include "server.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

static nlohmann::json make_event(const std::string& event, const nlohmann::json& data = nlohmann::json::object()) {
    nlohmann::json j;
    j["type"] = "event";
    j["event"] = event;
    j["data"] = data;
    return j;
}

static nlohmann::json make_error(const std::string& message) {
    nlohmann::json j;
    j["status"] = "error";
    j["message"] = message;
    return j;
}

static nlohmann::json make_ok(const std::string& message) {
    nlohmann::json j;
    j["status"] = "ok";
    j["message"] = message;
    return j;
}

Session::Session(tcp::socket socket_, ChessServer& server)
    : _socket(std::move(socket_))
    , _strand(_socket.get_executor())
    , _server(server) {}


ChessServer::ChessServer(boost::asio::io_context& io, const ServerSettings& config)
    : _acceptor(io, tcp::endpoint(boost::asio::ip::make_address(config.host), config.port))
    {
        BOOST_LOG_TRIVIAL(info) << "Server initialized on " << config.host << ":" << config.port;
        start_accept();
    }

tcp::socket& Session::socket(){
    return _socket;
}

void Session::start(){
    BOOST_LOG_TRIVIAL(info) << "New session started!";
    do_read();
}

void Session::send(const nlohmann::json& msg) {
    auto payload = Protocol::serialize_json(msg);
    auto self = shared_from_this();
    boost::asio::dispatch(_strand, [this, self, payload = std::move(payload)]() mutable {
        const bool writing = !_outbox.empty();
        _outbox.push_back(std::move(payload));
        if (!writing) do_write();
    });
}

void Session::close() {
    boost::system::error_code ignored;
    _socket.shutdown(tcp::socket::shutdown_both, ignored);
    _socket.close(ignored);
}

bool Session::is_authorized() const { return _authorized; }
const std::string& Session::nick() const { return _nick; }
int Session::rating() const { return _rating; }

void Session::handle_command(const std::string& raw, const GameCommand& cmd) {
    auto self = shared_from_this();

    if (cmd.type == "ping") {
        send(make_ok("pong"));
        return;
    }

    if (cmd.type == "auth") {
        _server.handle_auth(self, cmd);
        return;
    }
    if (cmd.type == "queue") {
        _server.handle_queue(self);
        return;
    }
    if (cmd.type == "leave") {
        _server.handle_leave(self);
        return;
    }
    if (cmd.type == "chat") {
        _server.handle_chat(self, cmd);
        return;
    }
    if (cmd.type == "move") {
        _server.handle_move(self, cmd);
        return;
    }
    if (cmd.type == "draw") {
        _server.handle_draw(self, cmd);
        return;
    }

    send(make_error("Unknown command type"));
}


// беск реккурсия (база реккурсия) (или цикл с break)

void Session::do_read() {
    auto self(shared_from_this());
    boost::asio::async_read_until(_socket, _buffer, '\n',
    [this, self](boost::system::error_code ec, std::size_t length) {
        if (!ec) {
            std::string data;
            std::istream is(&_buffer);
            std::getline(is, data);

            GameCommand cmd = Protocol::parse(data);

            if (cmd.is_valid) {
                BOOST_LOG_TRIVIAL(info) << "Valid command: " << cmd.type;
                handle_command(data, cmd);
            } else {
                BOOST_LOG_TRIVIAL(error) << "Invalid JSON received: " << data;
                send(make_error("Invalid format"));
            }
            do_read(); 
        } else {
            BOOST_LOG_TRIVIAL(info) << "Session read ended: " << ec.message();
            _server.on_disconnect(self);
        }
    });
}

void Session::do_write() {
    auto self = shared_from_this();
    boost::asio::async_write(
        _socket,
        boost::asio::buffer(_outbox.front()),
        boost::asio::bind_executor(
            _strand,
            [this, self](boost::system::error_code ec, std::size_t) {
                if (ec) {
                    BOOST_LOG_TRIVIAL(error) << "Session write error: " << ec.message();
                    _server.on_disconnect(self);
                    return;
                }
                _outbox.pop_front();
                if (!_outbox.empty()) do_write();
            }));
}

GameRoom::GameRoom(std::shared_ptr<Session> a, std::shared_ptr<Session> b)
    : _a(std::move(a)), _b(std::move(b)) {}

bool GameRoom::has(const std::shared_ptr<Session>& s) const {
    return s && (s == _a || s == _b);
}

std::shared_ptr<Session> GameRoom::opponent_of(const std::shared_ptr<Session>& s) const {
    if (!has(s)) return {};
    return (s == _a) ? _b : _a;
}

void GameRoom::start() {
    if (_a) _a->send(make_event("match_found", {{"opponent", _b ? _b->nick() : ""}}));
    if (_b) _b->send(make_event("match_found", {{"opponent", _a ? _a->nick() : ""}}));
}

void GameRoom::on_chat(const std::shared_ptr<Session>& from, const std::string& text) {
    BOOST_LOG_TRIVIAL(info) << "GameRoom::on_chat called, from: " << (from ? from->nick() : "null");
    
    auto other = opponent_of(from);
    BOOST_LOG_TRIVIAL(info) << "Opponent: " << (other ? other->nick() : "null");
    
    if (!other) {
        BOOST_LOG_TRIVIAL(error) << "No opponent found!";
        return;
    }
    other->send(make_event("chat", {{"from", from ? from->nick() : ""}, {"text", text}}));
}

void GameRoom::on_move(const std::shared_ptr<Session>& from, const std::string& from_sq, const std::string& to_sq) {
    auto other = opponent_of(from);
    if (!other) return;
    other->send(make_event("move", {{"from", from_sq}, {"to", to_sq}, {"by", from ? from->nick() : ""}}));
}

void GameRoom::on_draw_request(const std::shared_ptr<Session>& from) {
    if (!has(from)) return;
    if (from == _a) {
        _draw_offer_a = true;
        _draw_offer_b = false;
    }
    if (from == _b) {
        _draw_offer_b = true;
        _draw_offer_a = false;
    }

    auto other = opponent_of(from);
    if (other) {
        other->send(make_event("draw_offer", {{"from", from ? from->nick() : ""}}));
    }
}

bool GameRoom::on_draw_accept(const std::shared_ptr<Session>& from) {
    if (!has(from)) return false;
    const bool accepted =
        (from == _a && _draw_offer_b) ||
        (from == _b && _draw_offer_a);

    if (!accepted) return false;

    if (_a) {
        _a->send(make_event("draw_agreed"));
        _a->_room.reset();
    }
    if (_b) {
        _b->send(make_event("draw_agreed"));
        _b->_room.reset();
    }
    _draw_offer_a = false;
    _draw_offer_b = false;
    return true;
}

bool GameRoom::on_draw_decline(const std::shared_ptr<Session>& from) {
    if (!has(from)) return false;
    auto other = opponent_of(from);
    bool had_offer = false;
    if (from == _a && _draw_offer_b) {
        _draw_offer_b = false;
        had_offer = true;
    }
    if (from == _b && _draw_offer_a) {
        _draw_offer_a = false;
        had_offer = true;
    }
    if (had_offer && other) {
        other->send(make_event("draw_declined", {{"by", from ? from->nick() : ""}}));
    }
    return had_offer;
}

void GameRoom::on_disconnect(const std::shared_ptr<Session>& who) {
    auto other = opponent_of(who);
    if (other) {
        other->send(make_event("opponent_disconnected", {{"result", "win"}}));
        other->_room.reset();
    }
    if (_a) _a->_room.reset();
    if (_b) _b->_room.reset();
}

void ChessServer::start_accept(){

    auto new_session = std::make_shared<Session>(
        tcp::socket(static_cast<boost::asio::io_context&>(_acceptor.get_executor().context())),
        *this);

    _acceptor.async_accept(new_session->socket(),
[this, new_session](boost::system::error_code ec){
    if (!ec){
        _sessions.push_back(new_session);
        new_session->start(); // посмотреть на переполнение  stack ! 
    }
    else{
        BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
    }

    start_accept(); 
});
}

void ChessServer::on_disconnect(const std::shared_ptr<Session>& s) {
    if (!s) return;

    // Remove from queue if needed
    remove_from_queue(s);

    // If in a room - notify opponent
    if (auto room = s->_room.lock()) {
        auto winner = room->opponent_of(s);
        if (winner) {
            // Elo update on disconnect = win for remaining player
            const double Ra = static_cast<double>(winner->_rating);
            const double Rb = static_cast<double>(s->_rating);
            const double Ea = 1.0 / (1.0 + std::pow(10.0, (Rb - Ra) / 400.0));
            const double Eb = 1.0 - Ea;
            const int K = 32;
            winner->_rating = static_cast<int>(std::lround(Ra + K * (1.0 - Ea)));
            s->_rating = static_cast<int>(std::lround(Rb + K * (0.0 - Eb)));

            winner->send(make_event("rating_update", {{"rating", winner->_rating}}));
        }
        room->on_disconnect(s);
    }


    _sessions.erase(
        std::remove_if(_sessions.begin(), _sessions.end(),
                       [&](const std::shared_ptr<Session>& x) { return x == s; }),
        _sessions.end());

    s->close();
}

void ChessServer::handle_auth(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
    if (!s) return;
    if (cmd.nick.empty()) {
        s->send(make_error("nick required"));
        return;
    }
    s->_authorized = true;
    s->_nick = cmd.nick;
    s->send(make_ok("authorized"));
}

void ChessServer::handle_queue(const std::shared_ptr<Session>& s) {
    if (!s) return;
    if (!s->is_authorized()) {
        s->send(make_error("not authorized"));
        return;
    }
    if (!s->_room.expired()) {
        s->send(make_error("already in game"));
        return;
    }

    // Avoid duplicates in queue
    for (auto& q : _queue) {
        if (q == s) {
            s->send(make_ok("already queued"));
            return;
        }
    }

    _queue.push_back(s);
    BOOST_LOG_TRIVIAL(info) << "Player " << s->nick() << " added to queue. Queue size: " << _queue.size();
    s->send(make_ok("queued"));
    try_matchmake();
}

void ChessServer::handle_leave(const std::shared_ptr<Session>& s) {
    if (!s) return;
    remove_from_queue(s);
    if (auto room = s->_room.lock()) {
        room->on_disconnect(s);
        s->_room.reset();
    }
    s->send(make_ok("left"));
}

void ChessServer::handle_chat(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
    if (!s) return;
    if (!s->is_authorized()) {
        s->send(make_error("not authorized"));
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "handle_chat for " << s->nick() << ", checking room...";
    
    auto room = s->_room.lock();
    if (!room) {
        BOOST_LOG_TRIVIAL(error) << "Room is null for " << s->nick();
        s->send(make_error("not in game"));
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Room exists, calling on_chat";
    room->on_chat(s, cmd.text);
    s->send(make_ok("sent"));
}

void ChessServer::handle_move(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
    if (!s) return;
    if (!s->is_authorized()) {
        s->send(make_error("not authorized"));
        return;
    }
    auto room = s->_room.lock();
    if (!room) {
        s->send(make_error("not in game"));
        return;
    }
    room->on_move(s, cmd.from, cmd.to);
    s->send(make_ok("moved"));
}

void ChessServer::handle_draw(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
    if (!s) return;
    if (!s->is_authorized()) {
        s->send(make_error("not authorized"));
        return;
    }
    auto room = s->_room.lock();
    if (!room) {
        s->send(make_error("not in game"));
        return;
    }

    const std::string action = cmd.action.empty() ? "offer" : cmd.action;
    if (action == "offer") {
        room->on_draw_request(s);
        s->send(make_ok("draw offered"));
        return;
    }
    if (action == "accept") {
        if (room->on_draw_accept(s)) {
            s->send(make_ok("draw accepted"));
        } else {
            s->send(make_error("no draw offer to accept"));
        }
        return;
    }
    if (action == "decline") {
        if (room->on_draw_decline(s)) {
            s->send(make_ok("draw declined"));
        } else {
            s->send(make_error("no draw offer to decline"));
        }
        return;
    }
    s->send(make_error("unknown draw action"));
}

void ChessServer::remove_from_queue(const std::shared_ptr<Session>& s) {
    _queue.erase(std::remove(_queue.begin(), _queue.end(), s), _queue.end());
}

void ChessServer::try_matchmake() {
    BOOST_LOG_TRIVIAL(info) << "try_matchmake called, queue size: " << _queue.size();
    
    if (_queue.size() < 2) {
        BOOST_LOG_TRIVIAL(info) << "Not enough players in queue";
        return;
    }
    
    while (_queue.size() >= 2) {
        BOOST_LOG_TRIVIAL(info) << "Attempting to match players...";
        
        auto a = _queue.front();
        _queue.pop_front();
        if (!a) {
            BOOST_LOG_TRIVIAL(warning) << "Null session in queue, skipping";
            continue;
        }
        
        BOOST_LOG_TRIVIAL(info) << "First player: " << a->nick() << " rating: " << a->rating();

        int best_idx = -1;
        int best_diff = 0;
        for (int i = 0; i < static_cast<int>(_queue.size()); ++i) {
            auto& cand = _queue[i];
            if (!cand) continue;
            int diff = std::abs(cand->_rating - a->_rating);
            BOOST_LOG_TRIVIAL(info) << "  Checking candidate " << i << ": " << cand->nick() 
                                    << " rating: " << cand->rating() << " diff: " << diff;
            if (best_idx == -1 || diff < best_diff) {
                best_idx = i;
                best_diff = diff;
            }
        }

        if (best_idx == -1) {
            BOOST_LOG_TRIVIAL(info) << "No suitable opponent found for " << a->nick();
            _queue.push_front(a);
            return;
        }

        auto b = _queue[best_idx];
        _queue.erase(_queue.begin() + best_idx);
        if (!b) {
            BOOST_LOG_TRIVIAL(warning) << "Null candidate session, skipping";
            _queue.push_front(a);
            return;
        }

        BOOST_LOG_TRIVIAL(info) << "MATCH FOUND! " << a->nick() << " (rating: " << a->rating() 
                                << ") vs " << b->nick() << " (rating: " << b->rating() << ")";

        auto room = std::make_shared<GameRoom>(a, b);
        
        // Проверяем, что room создан
        if (!room) {
            BOOST_LOG_TRIVIAL(error) << "Failed to create GameRoom!";
            return;
        }
        
        a->_room = room;
        b->_room = room;
        
        BOOST_LOG_TRIVIAL(info) << "Room assigned to both players";
        
        _rooms.push_back(room);
        room->start();
        
        BOOST_LOG_TRIVIAL(info) << "Room started, queue size now: " << _queue.size();
    }
}


