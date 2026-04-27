// command.hpp
#pragma once 
#include <string>
#include "json.hpp"
#include "protocol.hpp"

struct GameCommand {
    std::string type;
    std::string from;
    std::string to;
    std::string nick;
    std::string password;
    std::string text;
    std::string action;
    std::string promotion;
    int player_id{};
    bool is_valid{false};

    static GameCommand from_protocol_message(const Protocol::Message& msg);
};

class ProtocolParser {
public:
    static GameCommand parse(const std::string& raw_data);
    static std::string serialize(const std::string& status, const std::string& msg);
    static std::string serialize_json(const nlohmann::json& jn);
};