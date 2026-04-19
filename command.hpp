// Работа с обработкой json файлов (ходы)

#pragma once 
#include <string>
#include "json.hpp"


struct GameCommand{

    std::string type; // "auth" "queue" "leave" "move" "chat" "draw" "ping"
    std::string from; // move: "e2"
    std::string to;   // move: "e4"
    std::string nick; // auth
    std::string password;
    std::string text; // chat
    std::string action; // draw: "offer"|"accept"|"decline"
    int player_id {};
    bool is_valid {false}; // Флаг парсинга

};

class Protocol{

    public:

        static GameCommand parse(const std::string& raw_d);

        static std::string serialize(const std::string& status, const std::string& msg);

        static std::string serialize_json(const nlohmann::json& jn);

};

