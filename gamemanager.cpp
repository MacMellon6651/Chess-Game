#include "gamemanager.hpp"
#include "server.hpp"


void GameManager::reg_player(int player_id, std::shared_ptr<Session> session){
    std::lock_guard<std::mutex> lock(_mutex);
    _active_sessions[player_id] = session;


    if (!_waiting_room.empty()) {
        int opponent_id = _waiting_room.back();
        _waiting_room.pop_back();
        start_match(opponent_id, player_id);
    } else {
        _waiting_room.push_back(player_id);
        session->do_write("{\"status\":\"waiting\", \"message\":\"Ожидание оппонента...\"}\n");
    }


}

void GameManager::del_player(int player_id){
    std::lock_guard<std::mutex> lock(_mutex);
    _active_sessions.erase(player_id);
}

void GameManager::send_to_player(int player_id, const std::string& msg){
    std::lock_guard<std::mutex> lock(_mutex);
    auto it = _active_sessions.find(player_id);
    if (it != _active_sessions.end()){
        it->second->do_write(msg);
    }
}


void GameManager::broadcast(const std::string& msg, int exclude_id){
    std::lock_guard<std::mutex> lock(_mutex);
    for (auto const& [id, session] : _active_sessions){
        if(id != exclude_id){
            session->do_write(msg);
        }
    }
}

void GameManager::start_match(int p1, int p2) {  
    _player_to_opponent[p1] = p2;
    _player_to_opponent[p2] = p1;

    // Уведомляем
    if (_active_sessions.count(p1)) 
        _active_sessions[p1]->do_write("{\"status\":\"match_started\", \"opponent\":" + std::to_string(p2) + ", \"color\":\"white\"}\n");
    
    if (_active_sessions.count(p2))
        _active_sessions[p2]->do_write("{\"status\":\"match_started\", \"opponent\":" + std::to_string(p1) + ", \"color\":\"black\"}\n");
}

void GameManager::handle_move(int sender_id, const std::string& from, const std::string& to) {
    std::lock_guard<std::mutex> lock(_mutex);

    // Ищем, есть ли у игрока оппонент
    if (_player_to_opponent.count(sender_id)) {
        int opponent_id = _player_to_opponent[sender_id];

        //  Формируем сообщение для оппонента
        std::string msg = "{\"type\":\"opponent_move\", \"from\":\"" + from + "\", \"to\":\"" + to + "\"}\n";

        // Отправляем ему
        if (_active_sessions.count(opponent_id)) {
            _active_sessions[opponent_id]->do_write(msg);
        }
    } else {
        if (_active_sessions.count(sender_id)) {
            _active_sessions[sender_id]->do_write("{\"status\":\"error\", \"message\":\"У вас еще нет оппонента!\"}\n");
        }
    }
}