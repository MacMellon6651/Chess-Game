// database.hpp
#pragma once

#include <sqlite3.h>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include "logging.hpp"

struct UserInfo {
    int id = -1;
    std::string nickname;
    int rating;
    int games_played;
    int games_won;
    int games_drawn;
};

struct QueuedPlayer {
    int user_id;
    std::string nickname;
    int rating;
};

class Database {
public:
    Database(const std::string& db_path = "chess.db");
    ~Database();
    
    bool register_user(const std::string& nickname, const std::string& password);
    std::optional<UserInfo> login_user(const std::string& nickname, const std::string& password);

    std::optional<UserInfo> login_or_register(const std::string& nickname, const std::string& password);
    
    bool user_exists(const std::string& nickname);
    
    void update_rating(int user_id, int new_rating);
    void update_stats(int user_id, bool won, bool draw = false);
    int get_rating(int user_id);
    UserInfo get_user_info(int user_id);
    
    void save_game(int white_id, int black_id, int winner_id, const std::string& result, const std::string& pgn = "");
    std::vector<std::string> get_user_game_history(int user_id, int limit = 10);
    
    void add_to_queue(int user_id);
    void remove_from_queue(int user_id);
    std::vector<QueuedPlayer> get_queue();
    void clear_queue();
    
    void update_last_login(int user_id);
    
private:
    bool execute_query(const std::string& sql);
    void create_tables();
    
    sqlite3* db_ = nullptr;
    std::mutex mutex_;
    
    std::string hash_password(const std::string& password);
    bool verify_password(const std::string& password, const std::string& hash);
};