#pragma once 

#include <map>
#include <memory>
#include <mutex>
#include <string>


class Session;

class GameManager {

    public:
        void reg_player(int player_id, std::shared_ptr<Session> session);

        void del_player(int player_id);

        void send_to_player(int player_id, const std::string& msg);
        
        void broadcast(const std::string& message, int exclude_id = -1);

    private:

        std::map<int, std::shared_ptr<Session>> _active_sessions;
        std::mutex _mutex;
};