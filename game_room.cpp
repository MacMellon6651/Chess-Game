#include "game_room.hpp"
#include "server.hpp"
#include "database.hpp"
#include "protocol.hpp"
#include "chess_engine/move_gen.hpp"
#include <cmath>
#include <algorithm>

uint8_t algebraic_to_index(const std::string& sq) {
    if (sq.length() < 2) return 255;
    int file = sq[0] - 'a';
    int rank = sq[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return 255;
    return rank * 8 + file;
}

GameRoom::GameRoom(std::shared_ptr<IPlayer> white, std::shared_ptr<IPlayer> black, ChessServer& server)
    : _white(white), _black(black), _server(server) {}

void GameRoom::start() {
    std::string fen = _position.get_fen();
    auto match_event = [fen](std::shared_ptr<IPlayer> p, const std::string& opponent, const std::string& color) {
        if (!p) return;
        nlohmann::json msg;
        msg["type"] = "event";
        msg["event"] = "match_found";
        msg["data"]["opponent"] = opponent;
        msg["data"]["board"]["fen"] = fen;
        msg["data"]["board"]["you_are"] = color;
        p->send(msg);
    };
    match_event(_white, _black ? _black->nick() : "", "white");
    match_event(_black, _white ? _white->nick() : "", "black");
}

void GameRoom::broadcast(const nlohmann::json& msg) {
    if (_white) _white->send(msg);
    if (_black) _black->send(msg);
}

void GameRoom::on_chat(std::shared_ptr<IPlayer> from, const std::string& text) {
    auto to = opponent_of(from);
    if (!to) return;
    nlohmann::json msg;
    msg["type"] = "event";
    msg["event"] = "chat";
    msg["data"]["from"] = from->nick();
    msg["data"]["text"] = text;
    to->send(msg);
}

bool GameRoom::on_move(std::shared_ptr<IPlayer> from, const std::string& from_sq,
                       const std::string& to_sq, const std::string& promotion) {
    bool is_white_turn = (_position.side_to_move() == PieceColor::White);
    if ((from == _white && !is_white_turn) || (from == _black && is_white_turn)) {
        from->send(Protocol::StatusResponse::error("Not your turn").to_json());
        return false;
    }

    uint8_t from_idx = algebraic_to_index(from_sq);
    uint8_t to_idx = algebraic_to_index(to_sq);
    if (from_idx == 255 || to_idx == 255) {
        from->send(Protocol::StatusResponse::error("Invalid square format").to_json());
        return false;
    }

    MoveList moves = _position.generate_moves();
    Move found_move;
    bool move_found = false;
    for (uint8_t i = 0; i < moves.size(); ++i) {
        const Move& m = moves[i];
        if (m.from == from_idx && m.to == to_idx) {
            if (m.flag >= Move::Flag::PromoteToKnight && m.flag <= Move::Flag::PromoteToQueen) {
                if (promotion.empty()) {
                    from->send(Protocol::StatusResponse::error("Promotion piece required (n/b/r/q)").to_json());
                    return false;
                }
                bool promotion_matches = false;
                switch (m.flag) {
                    case Move::Flag::PromoteToKnight: promotion_matches = (promotion == "n"); break;
                    case Move::Flag::PromoteToBishop: promotion_matches = (promotion == "b"); break;
                    case Move::Flag::PromoteToRook:   promotion_matches = (promotion == "r"); break;
                    case Move::Flag::PromoteToQueen:  promotion_matches = (promotion == "q"); break;
                    default: break;
                }
                if (!promotion_matches) continue;
            }
            found_move = m;
            move_found = true;
            break;
        }
    }
    if (!move_found) {
        from->send(Protocol::StatusResponse::error("Illegal move").to_json());
        return false;
    }

    if (!_position.apply_move(found_move)) {
        from->send(Protocol::StatusResponse::error("Move failed").to_json());
        return false;
    }

    nlohmann::json move_msg;
    move_msg["type"] = "event";
    move_msg["event"] = "move";
    move_msg["data"]["from"] = from_sq;
    move_msg["data"]["to"] = to_sq;
    move_msg["data"]["by"] = from->nick();
    move_msg["data"]["fen"] = _position.get_fen();
    broadcast(move_msg);

    if (_position.is_checkmate()) {
        std::shared_ptr<IPlayer> winner = (_position.side_to_move() == PieceColor::White) ? _black : _white;
        end_game("checkmate", winner);
    } else if (_position.is_stalemate()) {
        end_game("stalemate", nullptr);
    } else if (_position.is_threefold_repetition()) {
        end_game("threefold_repetition", nullptr);
    } else if (_position.is_fifty_move_rule()) {
        end_game("fifty_move_rule", nullptr);
    } else {
        if (MoveGen::is_check(_position.pieces(), _position.side_to_move())) {
            std::string side_str = (_position.side_to_move() == PieceColor::White) ? "white" : "black";
            nlohmann::json check_msg;
            check_msg["type"] = "event";
            check_msg["event"] = "check";
            check_msg["data"]["side"] = side_str;
            broadcast(check_msg);
        }
    }
    return true;
}

void GameRoom::end_game(const std::string& result, std::shared_ptr<IPlayer> winner) {
    if (winner) {
        auto loser = opponent_of(winner);
        if (loser) update_rating(winner, loser);
    }
    if (auto db = _server.db()) {
        int white_id = _white ? _white->user_id() : -1;
        int black_id = _black ? _black->user_id() : -1;
        int winner_id = winner ? winner->user_id() : -1;
        db->save_game(white_id, black_id, winner_id, result, _position.get_fen());
    }
    nlohmann::json game_over_msg;
    game_over_msg["type"] = "event";
    game_over_msg["event"] = "game_over";
    game_over_msg["data"]["result"] = result;
    if (winner) game_over_msg["data"]["winner"] = winner->nick();
    broadcast(game_over_msg);
    if (_white) _white->set_room({});
    if (_black) _black->set_room({});
}

void GameRoom::update_rating(std::shared_ptr<IPlayer> winner, std::shared_ptr<IPlayer> loser) {
    const int K = 32;
    double Ra = winner->rating();
    double Rb = loser->rating();
    double Ea = 1.0 / (1.0 + std::pow(10.0, (Rb - Ra) / 400.0));
    double Eb = 1.0 - Ea;
    int new_winner_rating = static_cast<int>(std::lround(Ra + K * (1.0 - Ea)));
    int new_loser_rating = static_cast<int>(std::lround(Rb + K * (0.0 - Eb)));
    winner->set_rating(new_winner_rating);
    loser->set_rating(new_loser_rating);
    if (auto db = _server.db()) {
        db->update_rating(winner->user_id(), new_winner_rating);
        db->update_rating(loser->user_id(), new_loser_rating);
        db->update_stats(winner->user_id(), true, false);
        db->update_stats(loser->user_id(), false, false);
    }
    nlohmann::json rating_msg;
    rating_msg["type"] = "event";
    rating_msg["event"] = "rating_update";
    rating_msg["data"]["rating"] = winner->rating();
    winner->send(rating_msg);
    rating_msg["data"]["rating"] = loser->rating();
    loser->send(rating_msg);
}

void GameRoom::on_draw_request(std::shared_ptr<IPlayer> from) {
    if (from == _white) _draw_offer_white = true;
    else if (from == _black) _draw_offer_black = true;
    auto to = opponent_of(from);
    if (to) {
        nlohmann::json msg;
        msg["type"] = "event";
        msg["event"] = "draw_offer";
        msg["data"]["from"] = from->nick();
        to->send(msg);
    }
}

bool GameRoom::on_draw_accept(std::shared_ptr<IPlayer> from) {
    if ((from == _white && _draw_offer_black) || (from == _black && _draw_offer_white)) {
        end_game("draw_agreed", nullptr);
        return true;
    }
    return false;
}

bool GameRoom::on_draw_decline(std::shared_ptr<IPlayer> from) {
    if (from == _white && _draw_offer_black) {
        _draw_offer_black = false;
        if (_black) {
            nlohmann::json msg;
            msg["type"] = "event";
            msg["event"] = "draw_declined";
            msg["data"]["by"] = from->nick();
            _black->send(msg);
        }
        return true;
    }
    if (from == _black && _draw_offer_white) {
        _draw_offer_white = false;
        if (_white) {
            nlohmann::json msg;
            msg["type"] = "event";
            msg["event"] = "draw_declined";
            msg["data"]["by"] = from->nick();
            _white->send(msg);
        }
        return true;
    }
    return false;
}

void GameRoom::on_disconnect(std::shared_ptr<IPlayer> who) {
    auto other = opponent_of(who);
    if (other) {
        nlohmann::json msg;
        msg["type"] = "event";
        msg["event"] = "opponent_disconnected";
        other->send(msg);
        update_rating(other, who);
        other->set_room({});
    }
    if (_white) _white->set_room({});
    if (_black) _black->set_room({});
}

bool GameRoom::has(std::shared_ptr<IPlayer> p) const {
    return p && (p == _white || p == _black);
}

std::shared_ptr<IPlayer> GameRoom::opponent_of(std::shared_ptr<IPlayer> p) const {
    if (!has(p)) return {};
    return (p == _white) ? _black : _white;
}

std::string GameRoom::get_position_fen() const {
    return _position.get_fen();
}