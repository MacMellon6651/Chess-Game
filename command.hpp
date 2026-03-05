// Работа с обработкой json файлов (ходы)

#pragma once 
#include <string>
#include "json.hpp"


struct GameCommand{

    std::string type; // "move" "join" "chat"
    std::string from; // "e2"
    std::string to; // "e4"
    int player_id {}; 
    bool is_valid {false}; // Флаг парсинга

};

class Protocol{

    public:

        static GameCommand parse(const std::string& raw_d);

        static std::string serialize(const std::string& status, const std::string& msg);

};

