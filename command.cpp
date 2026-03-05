#include "command.hpp"
#include <iostream>

GameCommand Protocol::parse(const std::string& raw_d){
    GameCommand cmd;
    try{

        auto jn = nlohmann::json::parse(raw_d);

        if (jn.contains("type")){
            cmd.type = jn.at("type").get<std::string>();
            cmd.is_valid = true;
        }

        if (jn.contains("move")){
            cmd.from = jn.value("from","");
            cmd.to = jn.value("to","");
        }

        cmd.player_id = jn.value("player_id", 0);

    }
    catch (const nlohmann::json::exception& err){
        cmd.is_valid = false;
    }
    return cmd;
}

std::string Protocol::serialize(const std::string& status, const std::string& msg){
    nlohmann::json jn;
    jn["status"] = status;
    jn["message"] = msg;
    return jn.dump() + "\n";
}