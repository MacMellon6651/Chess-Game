#pragma once

#include "json.hpp"
#include <string>
#include <memory>

class GameRoom;

class IPlayer {
public:
    virtual ~IPlayer() = default;

    virtual void send(const nlohmann::json& msg) = 0;
    virtual void close() = 0;

    virtual bool is_authorized() const = 0;
    virtual void set_authorized(bool auth) = 0;
    virtual const std::string& nick() const = 0;
    virtual void set_nick(const std::string& n) = 0;
    virtual int rating() const = 0;
    virtual void set_rating(int r) = 0;
    virtual int user_id() const = 0;
    virtual void set_user_id(int id) = 0;

    virtual std::weak_ptr<GameRoom> room() const = 0;
    virtual void set_room(std::weak_ptr<GameRoom> r) = 0;

    virtual bool in_queue() const = 0;
    virtual void set_in_queue(bool q) = 0;
};