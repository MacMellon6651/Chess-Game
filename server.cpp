#include "server.hpp"
#include "tcp_session.hpp"
#include "websocket_session.hpp"
#include "command.hpp"
#include "protocol.hpp"
#include "database.hpp"
#include <boost/beast/core.hpp>
#include "command.hpp" 
#include <algorithm>

ChessServer::ChessServer(boost::asio::io_context& io, const ServerSettings& config)
    : _io(io)
    , _tcp_acceptor(io, tcp::endpoint(boost::asio::ip::make_address(config.host), config.port))
    , _db(std::make_shared<Database>("chess.db")) {
    BOOST_LOG_TRIVIAL(info) << "Server initialized on " << config.host << ":" << config.port;
}

ChessServer::~ChessServer() {}

void ChessServer::start_accept_tcp() {
    _tcp_acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<TcpSession>(std::move(socket), *this);
                _all_players.push_back(session);
                session->start();
            }
            start_accept_tcp();
        });
}

void ChessServer::start_websocket_server() {
    const int ws_port = 18081; // можно вынести в settings.json
    _ws_acceptor = std::make_unique<tcp::acceptor>(_io,
        tcp::endpoint(boost::asio::ip::make_address("127.0.0.1"), ws_port));
    accept_websocket();
    BOOST_LOG_TRIVIAL(info) << "WebSocket Server started on port " << ws_port;
}

void ChessServer::accept_websocket() {
    _ws_acceptor->async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
                _all_players.push_back(session);
                session->start();
            }
            accept_websocket();
        });
}

void ChessServer::handle_auth(std::shared_ptr<IPlayer> player, const GameCommand& cmd) {
    if (cmd.nick.empty()) {
        player->send(Protocol::StatusResponse::error("nick required").to_json());
        return;
    }
    BOOST_LOG_TRIVIAL(info) << "Auth attempt: " << cmd.nick;
    auto user_info = _db->login_or_register(cmd.nick, cmd.password);
    if (user_info) {
        player->set_authorized(true);
        player->set_nick(user_info->nickname);
        player->set_rating(user_info->rating);
        player->set_user_id(user_info->id);
        player->send(Protocol::StatusResponse::ok("authorized").to_json());

        nlohmann::json rating_msg;
        rating_msg["type"] = "event";
        rating_msg["event"] = "rating_update";
        rating_msg["data"]["rating"] = player->rating();
        player->send(rating_msg);
    } else {
        player->send(Protocol::StatusResponse::error("Authentication failed").to_json());
    }
}

void ChessServer::handle_queue(std::shared_ptr<IPlayer> player) {
    if (!player->is_authorized()) {
        player->send(Protocol::StatusResponse::error("not authorized").to_json());
        return;
    }
    if (!player->room().expired()) {
        player->send(Protocol::StatusResponse::error("already in game").to_json());
        return;
    }
    if (player->in_queue()) {
        player->send(Protocol::StatusResponse::ok("already queued").to_json());
        return;
    }

    player->set_in_queue(true);
    _queue.push_back(player);
    player->send(Protocol::StatusResponse::ok("queued").to_json());
    broadcast_queue_size();
    try_matchmake();
}

void ChessServer::handle_leave(std::shared_ptr<IPlayer> player) {
    remove_from_queue(player);
    auto room = player->room().lock();
    if (room) room->on_disconnect(player);
    player->set_room({});
    player->send(Protocol::StatusResponse::ok("left").to_json());
}

void ChessServer::handle_chat(std::shared_ptr<IPlayer> player, const GameCommand& cmd) {
    if (!player->is_authorized()) {
        player->send(Protocol::StatusResponse::error("not authorized").to_json());
        return;
    }
    auto room = player->room().lock();
    if (!room) {
        player->send(Protocol::StatusResponse::error("not in game").to_json());
        return;
    }
    room->on_chat(player, cmd.text);
    player->send(Protocol::StatusResponse::ok("sent").to_json());
}

void ChessServer::handle_move(std::shared_ptr<IPlayer> player, const GameCommand& cmd) {
    if (!player->is_authorized()) {
        player->send(Protocol::StatusResponse::error("not authorized").to_json());
        return;
    }
    auto room = player->room().lock();
    if (!room) {
        player->send(Protocol::StatusResponse::error("not in game").to_json());
        return;
    }
    room->on_move(player, cmd.from, cmd.to, cmd.promotion);
}

void ChessServer::handle_draw(std::shared_ptr<IPlayer> player, const GameCommand& cmd) {
    if (!player->is_authorized()) {
        player->send(Protocol::StatusResponse::error("not authorized").to_json());
        return;
    }
    auto room = player->room().lock();
    if (!room) {
        player->send(Protocol::StatusResponse::error("not in game").to_json());
        return;
    }
    const std::string& action = cmd.action.empty() ? "offer" : cmd.action;
    if (action == "offer") {
        room->on_draw_request(player);
        player->send(Protocol::StatusResponse::ok("draw offered").to_json());
    } else if (action == "accept") {
        if (room->on_draw_accept(player))
            player->send(Protocol::StatusResponse::ok("draw accepted").to_json());
        else
            player->send(Protocol::StatusResponse::error("no draw offer to accept").to_json());
    } else if (action == "decline") {
        if (room->on_draw_decline(player))
            player->send(Protocol::StatusResponse::ok("draw declined").to_json());
        else
            player->send(Protocol::StatusResponse::error("no draw offer to decline").to_json());
    } else {
        player->send(Protocol::StatusResponse::error("unknown draw action").to_json());
    }
}

void ChessServer::handle_leaderboard(std::shared_ptr<IPlayer> player) {
    if (!player->is_authorized()) {
        player->send(Protocol::StatusResponse::error("not authorized").to_json());
        return;
    }
    auto top_players = _db->get_top_players(10);
    nlohmann::json msg;
    msg["type"] = "event";
    msg["event"] = "leaderboard";
    msg["data"]["players"] = top_players;
    player->send(msg);
}

void ChessServer::on_disconnect(std::shared_ptr<IPlayer> player) {
    remove_from_queue(player);
    auto room = player->room().lock();
    if (room) room->on_disconnect(player);
    _all_players.erase(std::remove(_all_players.begin(), _all_players.end(), player), _all_players.end());
    broadcast_queue_size();
}

void ChessServer::remove_from_queue(std::shared_ptr<IPlayer> player) {
    if (player->in_queue()) {
        player->set_in_queue(false);
        auto it = std::find(_queue.begin(), _queue.end(), player);
        if (it != _queue.end()) _queue.erase(it);
        broadcast_queue_size();
    }
}

void ChessServer::try_matchmake() {
    if (_queue.size() < 2) return;

    std::vector<std::shared_ptr<IPlayer>> candidates(_queue.begin(), _queue.end());
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a->rating() < b->rating(); });

    auto first = candidates[0];
    int best_diff = std::abs(candidates[1]->rating() - first->rating());
    size_t best_idx = 1;
    for (size_t i = 2; i < candidates.size(); ++i) {
        int diff = std::abs(candidates[i]->rating() - first->rating());
        if (diff < best_diff) {
            best_diff = diff;
            best_idx = i;
        }
    }

    auto white = first;
    auto black = candidates[best_idx];
    remove_from_queue(white);
    remove_from_queue(black);

    auto room = std::make_shared<GameRoom>(white, black, *this);
    white->set_room(room);
    black->set_room(room);
    _rooms.push_back(room);
    room->start();
}

void ChessServer::broadcast_queue_size() {
    nlohmann::json msg;
    msg["type"] = "event";
    msg["event"] = "queue_update";
    msg["data"]["size"] = static_cast<int>(_queue.size());
    for (auto& p : _all_players) {
        if (p && p->is_authorized()) p->send(msg);
    }
}