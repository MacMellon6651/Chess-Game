#include "command.hpp"
#include <iostream>

GameCommand GameCommand::from_protocol_message(const Protocol::Message& msg) {
    GameCommand cmd;
    
    if (const auto* auth = dynamic_cast<const Protocol::AuthRequest*>(&msg)) {
        cmd.type = "auth";
        cmd.nick = auth->nick;
        cmd.password = auth->password;
        cmd.is_valid = true;
    }
    else if (const auto* move = dynamic_cast<const Protocol::MoveRequest*>(&msg)) {
        cmd.type = "move";
        cmd.from = move->from;
        cmd.to = move->to;
        cmd.promotion = move->promotion;
        cmd.is_valid = true;
    }
    else if (const auto* chat = dynamic_cast<const Protocol::ChatRequest*>(&msg)) {
        cmd.type = "chat";
        cmd.text = chat->text;
        cmd.is_valid = true;
    }
    else if (const auto* draw = dynamic_cast<const Protocol::DrawRequest*>(&msg)) {
        cmd.type = "draw";
        cmd.action = draw->action;
        cmd.is_valid = true;
    }
    else if (dynamic_cast<const Protocol::QueueRequest*>(&msg)) {
        cmd.type = "queue";
        cmd.is_valid = true;
    }
    else if (dynamic_cast<const Protocol::LeaveRequest*>(&msg)) {
        cmd.type = "leave";
        cmd.is_valid = true;
    }
    else if (dynamic_cast<const Protocol::PingRequest*>(&msg)) {
        cmd.type = "ping";
        cmd.is_valid = true;
    }
    
    return cmd;
}

GameCommand ProtocolParser::parse(const std::string& raw_data) {
    try {
        auto j = nlohmann::json::parse(raw_data);
        auto msg = Protocol::MessageFactory::create(j);
        if (msg) {
            return GameCommand::from_protocol_message(*msg);
        }
    } catch (const nlohmann::json::exception&) {
        // Ошибка парсинга
    }
    
    GameCommand cmd;
    cmd.is_valid = false;
    return cmd;
}

std::string ProtocolParser::serialize(const std::string& status, const std::string& msg) {
    nlohmann::json jn;
    jn["status"] = status;
    jn["message"] = msg;
    return jn.dump() + "\n";
}

std::string ProtocolParser::serialize_json(const nlohmann::json& jn) {
    return jn.dump() + "\n";
}