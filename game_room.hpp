#pragma once

#include "player_interface.hpp"
#include "chess_engine/position.hpp"
#include <memory>
#include <string>

class ChessServer;

class GameRoom : public std::enable_shared_from_this<GameRoom> {
public:
    GameRoom(std::shared_ptr<IPlayer> white, std::shared_ptr<IPlayer> black, ChessServer& server);

    void start();
    void on_chat(std::shared_ptr<IPlayer> from, const std::string& text);
    bool on_move(std::shared_ptr<IPlayer> from, const std::string& from_sq,
                 const std::string& to_sq, const std::string& promotion = "");
    void on_draw_request(std::shared_ptr<IPlayer> from);
    bool on_draw_accept(std::shared_ptr<IPlayer> from);
    bool on_draw_decline(std::shared_ptr<IPlayer> from);
    void on_disconnect(std::shared_ptr<IPlayer> who);

    bool has(std::shared_ptr<IPlayer> p) const;
    std::shared_ptr<IPlayer> opponent_of(std::shared_ptr<IPlayer> p) const;
    std::string get_position_fen() const;

private:
    void broadcast(const nlohmann::json& msg);
    void end_game(const std::string& result, std::shared_ptr<IPlayer> winner = nullptr);
    void update_rating(std::shared_ptr<IPlayer> winner, std::shared_ptr<IPlayer> loser);

    std::shared_ptr<IPlayer> _white;
    std::shared_ptr<IPlayer> _black;
    bool _draw_offer_white = false;
    bool _draw_offer_black = false;
    Position _position;
    ChessServer& _server;
};