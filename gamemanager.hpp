#pragma once 

#include <map>
#include <memory>
#include <mutex>
#include <string>


class Session;


struct Match {
    int white_id;
    int black_id;
};

class GameManager {

    public:
        void reg_player(int player_id, std::shared_ptr<Session> session);

        void del_player(int player_id);

        void send_to_player(int player_id, const std::string& msg);
        

        void handle_move(int sender_id, const std::string& from, const std::string& to);


        void broadcast(const std::string& message, int exclude_id = -1);

    private:

        std::map<int, std::shared_ptr<Session>> _active_sessions;


        std::map<int, int> _player_to_opponent;
        std::vector<int> _waiting_room;
        std::mutex _mutex;

        void start_match(int p1, int p2);
};