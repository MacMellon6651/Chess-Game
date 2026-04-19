#include "server.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include "chess_engine/move_gen.hpp"
#include "database.hpp"

// Вспомогательные функции для преобразования координат
uint8_t algebraic_to_index(const std::string& sq) {
    if (sq.length() < 2) return 255;
    int file = sq[0] - 'a';
    int rank = sq[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return 255;
    return rank * 8 + file;
}

std::string index_to_algebraic(uint8_t idx) {
    if (idx >= 64) return "";
    char file = 'a' + (idx % 8);
    char rank = '1' + (idx / 8);
    return std::string({file, rank});
}

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

//  Session 

Session::Session(tcp::socket socket_, ChessServer& server)
    : _socket(std::move(socket_))
    , _strand(_socket.get_executor())
    , _server(server) {}

tcp::socket& Session::socket() { return _socket; }

void Session::start() {
    BOOST_LOG_TRIVIAL(info) << "New session started!";
    do_read();
}

void Session::send(const nlohmann::json& msg) {
    auto payload = Protocol::serialize_json(msg);
    auto self = shared_from_this();
    boost::asio::dispatch(_strand, [this, self, payload = std::move(payload)]() mutable {
        bool writing = !_outbox.empty();
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

//                  GameRoom

GameRoom::GameRoom(std::shared_ptr<Session> a, std::shared_ptr<Session> b, ChessServer& server)
    : _a(std::move(a)), _b(std::move(b)), _server(server) {}

void GameRoom::broadcast(const nlohmann::json& msg) {
    if (_a) _a->send(msg);
    if (_b) _b->send(msg);
}

void GameRoom::start() {
    // Отправляем начальную позицию доски
    nlohmann::json board_data;
    board_data["fen"] = _position.get_fen();
    board_data["you_are"] = "white";
    
    if (_a) {
        _a->send(make_event("match_found", {{"opponent", _b ? _b->nick() : ""}, {"board", board_data}}));
        board_data["you_are"] = "black";
    }
    if (_b) {
        _b->send(make_event("match_found", {{"opponent", _a ? _a->nick() : ""}, {"board", board_data}}));
    }
    
    BOOST_LOG_TRIVIAL(info) << "Game started between " << (_a ? _a->nick() : "?") 
                            << " and " << (_b ? _b->nick() : "?");
}

void GameRoom::on_chat(const std::shared_ptr<Session>& from, const std::string& text) {
    auto other = opponent_of(from);
    if (!other) return;
    other->send(make_event("chat", {{"from", from ? from->nick() : ""}, {"text", text}}));
}

bool GameRoom::on_move(const std::shared_ptr<Session>& from, const std::string& from_sq, 
                       const std::string& to_sq, const std::string& promotion) {
    // Определяем, чей сейчас ход
    uint8_t current_side = _position.side_to_move();
    uint8_t player_side = (from == _a) ? PieceColor::White : PieceColor::Black;
    
    // Проверяем, что ходит нужный игрок
    if (current_side != player_side) {
        from->send(make_error("Not your turn"));
        return false;
    }
    
    // Преобразуем координаты
    uint8_t from_idx = algebraic_to_index(from_sq);
    uint8_t to_idx = algebraic_to_index(to_sq);
    
    if (from_idx == 255 || to_idx == 255) {
        from->send(make_error("Invalid square format"));
        return false;
    }
    
    // Генерируем все легальные ходы
    MoveList moves = _position.generate_moves();
    
    // Ищем наш ход
    Move found_move;
    bool move_found = false;
    
    for (uint8_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        if (m.from == from_idx && m.to == to_idx) {
            // Проверка превращения пешки
            if (m.flag >= Move::Flag::PromoteToKnight && m.flag <= Move::Flag::PromoteToQueen) {
                if (promotion.empty()) {
                    from->send(make_error("Promotion piece required (n/b/r/q)"));
                    return false;
                }
                // Проверяем соответствие фигуры превращения
                if ((promotion == "n" && m.flag != Move::Flag::PromoteToKnight) ||
                    (promotion == "b" && m.flag != Move::Flag::PromoteToBishop) ||
                    (promotion == "r" && m.flag != Move::Flag::PromoteToRook) ||
                    (promotion == "q" && m.flag != Move::Flag::PromoteToQueen)) {
                    continue;
                }
            }
            found_move = m;
            move_found = true;
            break;
        }
    }
    
    if (!move_found) {
        from->send(make_error("Illegal move"));
        return false;
    }
    
    // Применяем ход
    if (!_position.apply_move(found_move)) {
        from->send(make_error("Move failed"));
        return false;
    }
    
    // Отправляем обновление обоим игрокам
    auto move_event = make_event("move", {
        {"from", from_sq},
        {"to", to_sq},
        {"by", from->nick()},
        {"fen", _position.get_fen()}
    });
    broadcast(move_event);
    
    // Проверяем окончание партии
    if (_position.is_checkmate()) {
        std::string winner = (_position.side_to_move() == PieceColor::White) ? 
                             (_b ? _b->nick() : "") : (_a ? _a->nick() : "");
        end_game("checkmate", winner.empty() ? nullptr : 
                 (winner == (_a ? _a->nick() : "") ? _a : _b));
        return true;
    }
    
    if (_position.is_stalemate()) {
        end_game("stalemate", nullptr);
        return true;
    }
    
    if (_position.is_threefold_repetition()) {
        end_game("threefold_repetition", nullptr);
        return true;
    }
    
    if (_position.is_fifty_move_rule()) {
        end_game("fifty_move_rule", nullptr);
        return true;
    }
    
    // Проверяем шах
    if (MoveGen::is_check(_position.pieces(), _position.side_to_move())) {
        broadcast(make_event("check", {{"side", _position.side_to_move() == PieceColor::White ? "white" : "black"}}));
    }
    
    return true;
}

void GameRoom::end_game(const std::string& result, const std::shared_ptr<Session>& winner) {
    nlohmann::json event_data;
    event_data["result"] = result;
    
    BOOST_LOG_TRIVIAL(info) << "=== GAME ENDED ===";
    BOOST_LOG_TRIVIAL(info) << "Result: " << result;
    
    if (winner) {
        event_data["winner"] = winner->nick();
        BOOST_LOG_TRIVIAL(info) << "Winner: " << winner->nick();
        
        // Обновление рейтинга
        auto loser = opponent_of(winner);
        if (loser) {
            update_rating(winner, loser);
        }
    }
    
    // ДОБАВЛЯЕМ СОХРАНЕНИЕ ИГРЫ В БД
    if (auto db = _server.get_db()) {
        int white_id = _a ? _a->_user_id : -1;
        int black_id = _b ? _b->_user_id : -1;
        int winner_id = winner ? winner->_user_id : -1;
        
        BOOST_LOG_TRIVIAL(info) << "Saving game: white_id=" << white_id 
                                << " black_id=" << black_id 
                                << " winner_id=" << winner_id;
        
        db->save_game(white_id, black_id, winner_id, result, _position.get_fen());
    } else {
        BOOST_LOG_TRIVIAL(error) << "Database not available, game not saved!";
    }
    
    broadcast(make_event("game_over", event_data));
    
    // Очищаем комнату у игроков
    if (_a) _a->_room.reset();
    if (_b) _b->_room.reset();
}

void GameRoom::update_rating(const std::shared_ptr<Session>& winner, const std::shared_ptr<Session>& loser) {
    // Формула Эло
    const int K = 32;
    double Ra = static_cast<double>(winner->_rating);
    double Rb = static_cast<double>(loser->_rating);
    double Ea = 1.0 / (1.0 + std::pow(10.0, (Rb - Ra) / 400.0));
    double Eb = 1.0 - Ea;
    
    int new_winner_rating = static_cast<int>(std::lround(Ra + K * (1.0 - Ea)));
    int new_loser_rating = static_cast<int>(std::lround(Rb + K * (0.0 - Eb)));
    
    winner->_rating = new_winner_rating;
    loser->_rating = new_loser_rating;
    
    // ДОБАВЛЯЕМ СОХРАНЕНИЕ РЕЙТИНГА В БД
    if (auto db = _server.get_db()) {
        db->update_rating(winner->_user_id, new_winner_rating);
        db->update_rating(loser->_user_id, new_loser_rating);
        db->update_stats(winner->_user_id, true, false);
        db->update_stats(loser->_user_id, false, false);
        BOOST_LOG_TRIVIAL(info) << "Ratings saved to database";
    } else {
        BOOST_LOG_TRIVIAL(error) << "Database not available, ratings not saved!";
    }
    
    winner->send(make_event("rating_update", {{"rating", winner->_rating}}));
    loser->send(make_event("rating_update", {{"rating", loser->_rating}}));
    
    BOOST_LOG_TRIVIAL(info) << "Rating updated: " << winner->nick() << " " << new_winner_rating
                            << ", " << loser->nick() << " " << new_loser_rating;
}

void GameRoom::on_draw_request(const std::shared_ptr<Session>& from) {
    if (!has(from)) return;
    
    if (from == _a) {
        _draw_offer_a = true;
    } else {
        _draw_offer_b = true;
    }
    
    auto other = opponent_of(from);
    if (other) {
        other->send(make_event("draw_offer", {{"from", from ? from->nick() : ""}}));
    }
}

bool GameRoom::on_draw_accept(const std::shared_ptr<Session>& from) {
    if (!has(from)) return false;
    
    bool accepted = (from == _a && _draw_offer_b) || (from == _b && _draw_offer_a);
    
    if (!accepted) return false;
    
    end_game("draw_agreed", nullptr);
    return true;
}

bool GameRoom::on_draw_decline(const std::shared_ptr<Session>& from) {
    if (!has(from)) return false;
    
    if (from == _a && _draw_offer_b) {
        _draw_offer_b = false;
        if (_b) _b->send(make_event("draw_declined", {{"by", from ? from->nick() : ""}}));
        return true;
    }
    if (from == _b && _draw_offer_a) {
        _draw_offer_a = false;
        if (_a) _a->send(make_event("draw_declined", {{"by", from ? from->nick() : ""}}));
        return true;
    }
    
    return false;
}

void GameRoom::on_disconnect(const std::shared_ptr<Session>& who) {
    auto other = opponent_of(who);
    if (other) {
        other->send(make_event("opponent_disconnected", {{"result", "win"}}));
        // Победитель получает рейтинг
        update_rating(other, who);
        other->_room.reset();
    }
    if (_a) _a->_room.reset();
    if (_b) _b->_room.reset();
}

bool GameRoom::has(const std::shared_ptr<Session>& s) const {
    return s && (s == _a || s == _b);
}

std::shared_ptr<Session> GameRoom::opponent_of(const std::shared_ptr<Session>& s) const {
    if (!has(s)) return {};
    return (s == _a) ? _b : _a;
}

std::string GameRoom::get_position_fen() const {
    return _position.get_fen();
}

//  ChessServer 

ChessServer::ChessServer(boost::asio::io_context& io, const ServerSettings& config)
    : _acceptor(io, tcp::endpoint(boost::asio::ip::make_address(config.host), config.port)),
     db_(std::make_shared<Database>("chess.db")) {
    BOOST_LOG_TRIVIAL(info) << "Server initialized on " << config.host << ":" << config.port;
    start_accept();
}

void ChessServer::start_accept() {
    auto new_session = std::make_shared<Session>(
    tcp::socket(_acceptor.get_executor()),
    *this);

    _acceptor.async_accept(new_session->socket(),
        [this, new_session](boost::system::error_code ec) {
            if (!ec) {
                _sessions.push_back(new_session);
                new_session->start();
            } else {
                BOOST_LOG_TRIVIAL(error) << "Accept error: " << ec.message();
            }
            start_accept();
        });
}

void ChessServer::on_disconnect(const std::shared_ptr<Session>& s) {
    if (!s) return;

    remove_from_queue(s);

    if (auto room = s->_room.lock()) {
        room->on_disconnect(s);
    }

    _sessions.erase(
        std::remove_if(_sessions.begin(), _sessions.end(),
                       [&](const std::shared_ptr<Session>& x) { return x == s; }),
        _sessions.end());

    s->close();
    BOOST_LOG_TRIVIAL(info) << "Session disconnected: " << s->nick();
}

void ChessServer::handle_auth(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
    if (!s) return;
    if (cmd.nick.empty()) {
        s->send(make_error("nick required"));
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Auth attempt for nick: " << cmd.nick;
    
    // ДОБАВЛЯЕМ РАБОТУ С БАЗОЙ ДАННЫХ
    if (db_) {
        // Пытаемся войти или зарегистрироваться
        auto user_info = db_->login_or_register(cmd.nick, cmd.password);
        
        if (user_info.has_value()) {
            s->_authorized = true;
            s->_nick = user_info->nickname;
            s->_rating = user_info->rating;
            s->_user_id = user_info->id;
            s->send(make_ok("authorized"));
            s->send(make_event("rating_update", {{"rating", s->_rating}}));
            BOOST_LOG_TRIVIAL(info) << "Player authorized: " << s->nick() 
                                    << " (id: " << s->_user_id << ") rating: " << s->rating();
        } else {
            s->send(make_error("Authentication failed"));
            BOOST_LOG_TRIVIAL(error) << "Authentication failed for: " << cmd.nick;
        }
    } else {
        // Если БД не работает - старый способ (debug)
        BOOST_LOG_TRIVIAL(warning) << "Database not available, using fallback auth";
        s->_authorized = true;
        s->_nick = cmd.nick;
        s->send(make_ok("authorized"));
    }
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
    BOOST_LOG_TRIVIAL(info) << "Player " << s->nick() << " left queue/game";
}

void ChessServer::handle_chat(const std::shared_ptr<Session>& s, const GameCommand& cmd) {
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
    
    std::string promotion = cmd.action; // для превращения пешки
    if (room->on_move(s, cmd.from, cmd.to, promotion)) {
        // Ход успешно выполнен
    }
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
    
    while (_queue.size() >= 2) {
        auto a = _queue.front();
        _queue.pop_front();
        if (!a) continue;

        int best_idx = -1;
        int best_diff = 0;
        for (int i = 0; i < static_cast<int>(_queue.size()); ++i) {
            auto& cand = _queue[i];
            if (!cand || cand == a) continue;
            int diff = std::abs(cand->_rating - a->_rating);
            if (best_idx == -1 || diff < best_diff) {
                best_idx = i;
                best_diff = diff;
            }
        }

        if (best_idx == -1) {
            _queue.push_front(a);
            BOOST_LOG_TRIVIAL(info) << "No suitable opponent found for " << a->nick();
            return;
        }

        auto b = _queue[best_idx];
        _queue.erase(_queue.begin() + best_idx);
        if (!b) {
            _queue.push_front(a);
            return;
        }

        BOOST_LOG_TRIVIAL(info) << "Matched " << a->nick() << " (rating: " << a->rating() 
                                << ") with " << b->nick() << " (rating: " << b->rating() << ")";

        auto room = std::make_shared<GameRoom>(a, b, *this);
        a->_room = room;
        b->_room = room;
        _rooms.push_back(room);
        room->start();
    }
}

// server.cpp - добавить в конец файла

// ============================================================================
// WebSocket обработчики
// ============================================================================

void ChessServer::handle_websocket_auth(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd) {
    if (cmd.nick.empty()) {
        ws->send({{"status", "error"}, {"message", "nick required"}});
        return;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Auth attempt for: " << cmd.nick;
    
    // Пытаемся войти
    auto user_info = db_->login_user(cmd.nick, cmd.password);
    
    // Если пользователь не найден - регистрируем
    if (!user_info.has_value()) {
        BOOST_LOG_TRIVIAL(info) << "User not found, registering: " << cmd.nick;
        if (db_->register_user(cmd.nick, cmd.password)) {
            // После регистрации пробуем войти снова
            user_info = db_->login_user(cmd.nick, cmd.password);
        }
    }
    
    if (user_info.has_value()) {
        ws->_authorized = true;
        ws->_nick = user_info->nickname;
        ws->_rating = user_info->rating;
        ws->_user_id = user_info->id;
        
        ws->send({{"status", "ok"}, {"message", "authorized"}});
        ws->send(make_event("rating_update", {{"rating", ws->_rating}}));
        
        BOOST_LOG_TRIVIAL(info) << "WebSocket player authorized: " << ws->_nick 
                                << " (id: " << ws->_user_id << ") rating: " << ws->_rating;
    } else {
        ws->send({{"status", "error"}, {"message", "Authentication failed"}});
        BOOST_LOG_TRIVIAL(error) << "Authentication failed for: " << cmd.nick;
    }
}

void ChessServer::handle_websocket_queue(const std::shared_ptr<WebSocketSession>& ws) {
    if (!ws->is_authorized()) {
        ws->send({{"status", "error"}, {"message", "not authorized"}});
        return;
    }
    
    // Создаём или получаем TCP сессию для этого WebSocket
    auto session = get_or_create_session(ws);
    handle_queue(session);
}

void ChessServer::handle_websocket_leave(const std::shared_ptr<WebSocketSession>& ws) {
    if (!ws->is_authorized()) return;
    
    auto session = get_or_create_session(ws);
    if (session) {
        handle_leave(session);
    }
}

void ChessServer::handle_websocket_chat(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd) {
    if (!ws->is_authorized()) return;
    
    auto session = get_or_create_session(ws);
    if (session) {
        handle_chat(session, cmd);
    }
}

void ChessServer::handle_websocket_move(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd) {
    if (!ws->is_authorized()) return;
    
    auto session = get_or_create_session(ws);
    if (session) {
        handle_move(session, cmd);
    }
}

void ChessServer::handle_websocket_draw(const std::shared_ptr<WebSocketSession>& ws, const GameCommand& cmd) {
    if (!ws->is_authorized()) return;
    
    auto session = get_or_create_session(ws);
    handle_draw(session, cmd);
}

void ChessServer::on_websocket_disconnect(const std::shared_ptr<WebSocketSession>& ws) {
    if (ws && ws->is_authorized()) {
        auto session = get_or_create_session(ws);
        on_disconnect(session);
        
        // Удаляем из списка сессий
        _web_sessions.erase(
            std::remove_if(_web_sessions.begin(), _web_sessions.end(),
                [&](const std::shared_ptr<WebSocketSession>& x) { return x == ws; }),
            _web_sessions.end());
        
        // Удаляем из маппинга
        _nick_to_session.erase(ws->_nick);
    }
}

std::shared_ptr<Session> ChessServer::get_or_create_session(const std::shared_ptr<WebSocketSession>& ws) {
    // Проверяем, есть ли уже TCP сессия для этого пользователя
    auto it = _nick_to_session.find(ws->_nick);
    if (it != _nick_to_session.end()) {
        BOOST_LOG_TRIVIAL(info) << "Found existing session for: " << ws->_nick;
        return it->second;
    }
    
    BOOST_LOG_TRIVIAL(info) << "Creating new session for: " << ws->_nick;
    
    // Исправленный способ создания сокета
    // Получаем io_context из acceptor
    boost::asio::io_context& io_context = static_cast<boost::asio::io_context&>(_acceptor.get_executor().context());
    
    // Создаём сокет напрямую с io_context
    auto socket = std::make_unique<tcp::socket>(io_context);
    
    // Создаём сессию с перемещённым сокетом
    auto session = std::make_shared<Session>(std::move(*socket), *this);
    
    session->_authorized = true;
    session->_nick = ws->_nick;
    session->_rating = ws->_rating;
    session->_user_id = ws->_user_id;
    
    // Сохраняем в маппинг
    _nick_to_session[ws->_nick] = session;
    _sessions.push_back(session);
    
    // ВАЖНО: запускаем сессию
    session->start();
    
    return session;
}