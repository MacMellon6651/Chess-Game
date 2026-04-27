// protocol.hpp
#pragma once
#include "json.hpp"
#include <string>
#include <memory>

namespace Protocol {

// ============================================================================
// Базовые классы
// ============================================================================

struct Message {
    virtual ~Message() = default;
    virtual nlohmann::json to_json() const = 0;
    virtual bool from_json(const nlohmann::json& j) = 0;
};

// ============================================================================
// Запросы (от клиента к серверу)
// ============================================================================

struct AuthRequest : Message {
    std::string nick;
    std::string password;
    
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "auth";
        j["nick"] = nick;
        if (!password.empty()) j["password"] = password;
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        if (!j.contains("type") || j["type"] != "auth") return false;
        nick = j.value("nick", "");
        password = j.value("password", "");
        return !nick.empty();
    }
};

struct MoveRequest : Message {
    std::string from;
    std::string to;
    std::string promotion;
    
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "move";
        j["from"] = from;
        j["to"] = to;
        if (!promotion.empty()) j["promotion"] = promotion;
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        if (!j.contains("type") || j["type"] != "move") return false;
        from = j.value("from", "");
        to = j.value("to", "");
        promotion = j.value("promotion", "");
        return !from.empty() && !to.empty();
    }
};

struct ChatRequest : Message {
    std::string text;
    
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "chat";
        j["text"] = text;
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        if (!j.contains("type") || j["type"] != "chat") return false;
        text = j.value("text", "");
        return !text.empty();
    }
};

struct DrawRequest : Message {
    std::string action;
    
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "draw";
        j["action"] = action;
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        if (!j.contains("type") || j["type"] != "draw") return false;
        action = j.value("action", "");
        return !action.empty();
    }
};

struct QueueRequest : Message {
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "queue";
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        return j.contains("type") && j["type"] == "queue";
    }
};

struct LeaveRequest : Message {
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "leave";
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        return j.contains("type") && j["type"] == "leave";
    }
};

struct PingRequest : Message {
    nlohmann::json to_json() const override {
        nlohmann::json j;
        j["type"] = "ping";
        return j;
    }
    
    bool from_json(const nlohmann::json& j) override {
        return j.contains("type") && j["type"] == "ping";
    }
};

// ============================================================================
// Ответы (от сервера к клиенту)
// ============================================================================

struct StatusResponse {
    std::string status;
    std::string message;
    nlohmann::json data;
    
    static StatusResponse ok(const std::string& msg = "", const nlohmann::json& data = {}) {
        return {"ok", msg, data};
    }
    
    static StatusResponse error(const std::string& msg, const nlohmann::json& data = {}) {
        return {"error", msg, data};
    }
    
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["status"] = status;
        j["message"] = message;
        if (!data.empty()) j["data"] = data;
        return j;
    }
    
    static StatusResponse from_json(const nlohmann::json& j) {
        return {j.value("status", "error"), 
                j.value("message", ""), 
                j.value("data", nlohmann::json::object())};
    }
};

// ============================================================================
// События (от сервера к клиенту)
// ============================================================================

struct GameEvent {
    std::string event_name;
    nlohmann::json data;
    
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["type"] = "event";
        j["event"] = event_name;
        j["data"] = data;
        return j;
    }
    
    static GameEvent from_json(const nlohmann::json& j) {
        return {j.value("event", ""), j.value("data", nlohmann::json::object())};
    }
};

// ============================================================================
// Фабрика сообщений
// ============================================================================

class MessageFactory {
public:
    static std::unique_ptr<Message> create(const nlohmann::json& j) {
        if (!j.contains("type")) return nullptr;
        
        std::string type = j["type"];
        
        if (type == "auth") {
            auto msg = std::make_unique<AuthRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "move") {
            auto msg = std::make_unique<MoveRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "chat") {
            auto msg = std::make_unique<ChatRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "draw") {
            auto msg = std::make_unique<DrawRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "queue") {
            auto msg = std::make_unique<QueueRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "leave") {
            auto msg = std::make_unique<LeaveRequest>();
            msg->from_json(j);
            return msg;
        }
        if (type == "ping") {
            auto msg = std::make_unique<PingRequest>();
            msg->from_json(j);
            return msg;
        }
        
        return nullptr;
    }
};

// ============================================================================
// Вспомогательные функции для создания событий
// ============================================================================

inline GameEvent make_match_found_event(const std::string& opponent, 
                                         const std::string& fen, 
                                         const std::string& you_are) {
    GameEvent ev;
    ev.event_name = "match_found";
    ev.data["opponent"] = opponent;
    ev.data["board"]["fen"] = fen;
    ev.data["board"]["you_are"] = you_are;
    return ev;
}

inline GameEvent make_move_event(const std::string& from, const std::string& to,
                                  const std::string& by, const std::string& fen) {
    GameEvent ev;
    ev.event_name = "move";
    ev.data["from"] = from;
    ev.data["to"] = to;
    ev.data["by"] = by;
    ev.data["fen"] = fen;
    return ev;
}

inline GameEvent make_game_over_event(const std::string& result, const std::string& winner = "") {
    GameEvent ev;
    ev.event_name = "game_over";
    ev.data["result"] = result;
    if (!winner.empty()) ev.data["winner"] = winner;
    return ev;
}

inline GameEvent make_chat_event(const std::string& from, const std::string& text) {
    GameEvent ev;
    ev.event_name = "chat";
    ev.data["from"] = from;
    ev.data["text"] = text;
    return ev;
}

inline GameEvent make_draw_offer_event(const std::string& from) {
    GameEvent ev;
    ev.event_name = "draw_offer";
    ev.data["from"] = from;
    return ev;
}

inline GameEvent make_rating_update_event(int rating) {
    GameEvent ev;
    ev.event_name = "rating_update";
    ev.data["rating"] = rating;
    return ev;
}

inline GameEvent make_check_event(const std::string& side) {
    GameEvent ev;
    ev.event_name = "check";
    ev.data["side"] = side;
    return ev;
}

inline GameEvent make_opponent_disconnected_event() {
    GameEvent ev;
    ev.event_name = "opponent_disconnected";
    return ev;
}

} // namespace Protocol