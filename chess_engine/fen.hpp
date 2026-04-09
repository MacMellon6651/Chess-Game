#pragma once
#include "position.hpp"
#include <string>

// Парсер FEN-нотации
namespace FEN {
    // Парсинг полной FEN строки
    Position parse(const std::string& fen);
    
    // Генерация FEN строки из позиции
    std::string to_string(const Position& pos);
    
    // Проверка валидности FEN
    bool is_valid(const std::string& fen);
}