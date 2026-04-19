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

        cmd.from = jn.value("from","");
        cmd.to = jn.value("to","");
        cmd.nick = jn.value("nick","");
        cmd.password = jn.value("password", "");
        cmd.text = jn.value("text","");
        cmd.action = jn.value("action", "");

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

std::string Protocol::serialize_json(const nlohmann::json& jn) {
    return jn.dump() + "\n";
}