#include "gamemanager.hpp"
#include "server.hpp"


void GameManager::reg_player(int player_id, std::shared_ptr<Session> session){
    std::lock_guard<std::mutex> lock(_mutex);
    _active_sessions[player_id] = session;
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