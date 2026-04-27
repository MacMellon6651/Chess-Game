// database.cpp
#include "database.hpp"
#include <sstream>
#include <functional>

Database::Database(const std::string& db_path) {
    int rc = sqlite3_open(db_path.c_str(), &db_);
    if (rc) {
        BOOST_LOG_TRIVIAL(error) << "Can't open database: " << sqlite3_errmsg(db_);
        db_ = nullptr;
    } else {
        BOOST_LOG_TRIVIAL(info) << "Connected to SQLite database: " << db_path;
        create_tables();
    }
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool Database::execute_query(const std::string& sql) {
    if (!db_) return false;
    
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        BOOST_LOG_TRIVIAL(error) << "SQL error: " << errmsg;
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void Database::create_tables() {
    const std::string sql = 
        "CREATE TABLE IF NOT EXISTS users ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    nickname TEXT UNIQUE NOT NULL,"
        "    password_hash TEXT NOT NULL,"
        "    rating INTEGER DEFAULT 1000,"
        "    games_played INTEGER DEFAULT 0,"
        "    games_won INTEGER DEFAULT 0,"
        "    games_drawn INTEGER DEFAULT 0,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    last_login DATETIME"
        ");"
        "CREATE TABLE IF NOT EXISTS games ("
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    white_player_id INTEGER,"
        "    black_player_id INTEGER,"
        "    winner_id INTEGER,"
        "    result TEXT,"
        "    pgn TEXT,"
        "    played_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");"
        "CREATE TABLE IF NOT EXISTS game_queue ("
        "    user_id INTEGER PRIMARY KEY,"
        "    joined_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ");";
    
    execute_query(sql);
}

std::string Database::hash_password(const std::string& password) {
    std::hash<std::string> hasher;
    std::stringstream ss;
    ss << std::hex << hasher(password);
    return ss.str();
}

bool Database::verify_password(const std::string& password, const std::string& hash) {
    return hash_password(password) == hash;
}

bool Database::register_user(const std::string& nickname, const std::string& password) {
    BOOST_LOG_TRIVIAL(info) << "=== REGISTER_USER CALLED ===";
    BOOST_LOG_TRIVIAL(info) << "Nickname: " << nickname;
    BOOST_LOG_TRIVIAL(info) << "Password: " << password;
    
    if (user_exists(nickname)) {
        BOOST_LOG_TRIVIAL(warning) << "User already exists: " << nickname;
        return false;
    }
    
    std::string password_hash = hash_password(password);
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = "INSERT INTO users (nickname, password_hash) VALUES ('" + 
                      nickname + "', '" + password_hash + "')";
    
    BOOST_LOG_TRIVIAL(info) << "SQL: " << sql;
    
    if (execute_query(sql)) {
        BOOST_LOG_TRIVIAL(info) << "User registered successfully: " << nickname;
        return true;
    }
    BOOST_LOG_TRIVIAL(error) << "Failed to register user: " << nickname;
    return false;
}

std::optional<UserInfo> Database::login_or_register(const std::string& nickname, const std::string& password) {
    // Сначала пробуем войти
    auto user = login_user(nickname, password);
    if (user.has_value()) {
        return user;
    }
    
    // Если не получилось - регистрируем
    if (register_user(nickname, password)) {
        return login_user(nickname, password);
    }
    
    return std::nullopt;
}

std::optional<UserInfo> Database::login_user(const std::string& nickname, const std::string& password) {
    BOOST_LOG_TRIVIAL(info) << "Login attempt for: " << nickname;
    
    if (!db_) {
        BOOST_LOG_TRIVIAL(error) << "Database not connected";
        return std::nullopt;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string sql = "SELECT id, nickname, rating, games_played, games_won, games_drawn, password_hash "
                      "FROM users WHERE nickname = '" + nickname + "'";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        BOOST_LOG_TRIVIAL(error) << "SQL prepare failed: " << sqlite3_errmsg(db_);
        return std::nullopt;
    }
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        BOOST_LOG_TRIVIAL(info) << "User not found: " << nickname;
        return std::nullopt;
    }
    
    std::string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    
    if (!verify_password(password, stored_hash)) {
        sqlite3_finalize(stmt);
        BOOST_LOG_TRIVIAL(warning) << "Invalid password for: " << nickname;
        return std::nullopt;
    }
    
    UserInfo info;
    info.id = sqlite3_column_int(stmt, 0);
    info.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    info.rating = sqlite3_column_int(stmt, 2);
    info.games_played = sqlite3_column_int(stmt, 3);
    info.games_won = sqlite3_column_int(stmt, 4);
    info.games_drawn = sqlite3_column_int(stmt, 5);
    
    sqlite3_finalize(stmt);
    update_last_login(info.id);
    
    BOOST_LOG_TRIVIAL(info) << "User logged in: " << nickname << " rating: " << info.rating;
    return info;
}

bool Database::user_exists(const std::string& nickname) {
    if (!db_) return false;
    
    std::string sql = "SELECT 1 FROM users WHERE nickname = '" + nickname + "'";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return false;
    
    rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    sqlite3_finalize(stmt);
    return exists;
}

void Database::update_rating(int user_id, int new_rating) {
    std::string sql = "UPDATE users SET rating = " + std::to_string(new_rating) + 
                      " WHERE id = " + std::to_string(user_id);
    execute_query(sql);
}

void Database::update_stats(int user_id, bool won, bool draw) {
    std::string sql;
    if (won) {
        sql = "UPDATE users SET games_played = games_played + 1, "
              "games_won = games_won + 1 WHERE id = " + std::to_string(user_id);
    } else if (draw) {
        sql = "UPDATE users SET games_played = games_played + 1, "
              "games_drawn = games_drawn + 1 WHERE id = " + std::to_string(user_id);
    } else {
        sql = "UPDATE users SET games_played = games_played + 1 WHERE id = " + std::to_string(user_id);
    }
    execute_query(sql);
}

int Database::get_rating(int user_id) {
    if (!db_) return 1000;
    
    std::string sql = "SELECT rating FROM users WHERE id = " + std::to_string(user_id);
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return 1000;
    
    rc = sqlite3_step(stmt);
    int rating = 1000;
    if (rc == SQLITE_ROW) {
        rating = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return rating;
}

UserInfo Database::get_user_info(int user_id) {
    UserInfo info;
    if (!db_) return info;
    
    std::string sql = "SELECT id, nickname, rating, games_played, games_won, games_drawn "
                      "FROM users WHERE id = " + std::to_string(user_id);
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return info;
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        info.id = sqlite3_column_int(stmt, 0);
        info.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        info.rating = sqlite3_column_int(stmt, 2);
        info.games_played = sqlite3_column_int(stmt, 3);
        info.games_won = sqlite3_column_int(stmt, 4);
        info.games_drawn = sqlite3_column_int(stmt, 5);
    }
    sqlite3_finalize(stmt);
    return info;
}

void Database::save_game(int white_id, int black_id, int winner_id, 
                         const std::string& result, const std::string& pgn) {
    BOOST_LOG_TRIVIAL(info) << "=== SAVE GAME CALLED ===";
    BOOST_LOG_TRIVIAL(info) << "white_id: " << white_id;
    BOOST_LOG_TRIVIAL(info) << "black_id: " << black_id;
    BOOST_LOG_TRIVIAL(info) << "winner_id: " << winner_id;
    BOOST_LOG_TRIVIAL(info) << "result: " << result;
    
    std::string sql = "INSERT INTO games (white_player_id, black_player_id, winner_id, result, pgn) "
                      "VALUES (" + std::to_string(white_id) + ", " + 
                      std::to_string(black_id) + ", " + 
                      std::to_string(winner_id) + ", '" + result + "', '" + pgn + "')";
    
    BOOST_LOG_TRIVIAL(info) << "SQL: " << sql;
    
    if (execute_query(sql)) {
        BOOST_LOG_TRIVIAL(info) << "Game saved successfully!";
    } else {
        BOOST_LOG_TRIVIAL(error) << "Failed to save game!";
    }
}

std::vector<std::string> Database::get_user_game_history(int user_id, int limit) {
    std::vector<std::string> history;
    if (!db_) return history;
    
    std::string sql = "SELECT result, played_at FROM games "
                      "WHERE white_player_id = " + std::to_string(user_id) + 
                      " OR black_player_id = " + std::to_string(user_id) +
                      " ORDER BY played_at DESC LIMIT " + std::to_string(limit);
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return history;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string line = std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))) + 
                           " - " + reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        history.push_back(line);
    }
    sqlite3_finalize(stmt);
    return history;
}

void Database::add_to_queue(int user_id) {
    std::string sql = "INSERT OR REPLACE INTO game_queue (user_id, joined_at) VALUES (" + 
                      std::to_string(user_id) + ", CURRENT_TIMESTAMP)";
    execute_query(sql);
}

void Database::remove_from_queue(int user_id) {
    std::string sql = "DELETE FROM game_queue WHERE user_id = " + std::to_string(user_id);
    execute_query(sql);
}

std::vector<QueuedPlayer> Database::get_queue() {
    std::vector<QueuedPlayer> queue;
    if (!db_) return queue;
    
    std::string sql = "SELECT q.user_id, u.nickname, u.rating "
                      "FROM game_queue q JOIN users u ON q.user_id = u.id "
                      "ORDER BY q.joined_at";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) return queue;
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QueuedPlayer p;
        p.user_id = sqlite3_column_int(stmt, 0);
        p.nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p.rating = sqlite3_column_int(stmt, 2);
        queue.push_back(p);
    }
    sqlite3_finalize(stmt);
    return queue;
}

void Database::clear_queue() {
    execute_query("DELETE FROM game_queue");
}

void Database::update_last_login(int user_id) {
    std::string sql = "UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = " + std::to_string(user_id);
    execute_query(sql);
}

std::vector<nlohmann::json> Database::get_top_players(int limit) {
    std::vector<nlohmann::json> result;
    std::string sql = "SELECT id, nickname, rating FROM users ORDER BY rating DESC LIMIT " + std::to_string(limit);
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        return result;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        nlohmann::json p;
        p["id"] = sqlite3_column_int(stmt, 0);
        p["nickname"] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        p["rating"] = sqlite3_column_int(stmt, 2);
        result.push_back(p);
    }
    sqlite3_finalize(stmt);
    return result;
}