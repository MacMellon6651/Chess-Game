#include "fen.hpp"
#include <sstream>
#include <cctype>

Position FEN::parse(const std::string& fen) {
    std::istringstream iss(fen);
    std::string board_part;
    
    iss >> board_part;
    
    // Пока создаём только по части с фигурами
    return Position(board_part);
}

std::string FEN::to_string(const Position& pos) {
    return pos.get_fen();
}

bool FEN::is_valid(const std::string& fen) {
    if (fen.empty()) return false;
    
    // Простая проверка: есть хотя бы одна буква
    for (char c : fen) {
        if (std::isalpha(c)) return true;
    }
    return false;
}