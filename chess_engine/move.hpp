#pragma once
#include <cstdint>
#include <string>
#include "pieces.hpp"

// Структура для хранения одного хода
struct Move {
    uint8_t from{0};          // Откуда (0-63)
    uint8_t to{0};            // Куда (0-63)
    
    uint8_t attacker_type{255};  // Тип атакующей фигуры
    uint8_t attacker_side{255};  // Сторона атакующей фигуры
    
    uint8_t defender_type{255};  // Тип защитника (255 если нет)
    uint8_t defender_side{255};  // Сторона защитника (255 если нет)
    
    uint8_t flag{0};          // Специальный флаг хода
    
    // Флаги для специальных ходов
    struct Flag {
        static constexpr uint8_t Default = 0;
        static constexpr uint8_t PawnLongMove = 1;       // Длинный ход пешки (на 2 клетки)
        static constexpr uint8_t EnPassantCapture = 2;   // Взятие на проходе
        
        static constexpr uint8_t WhiteLongCastling = 3;   // Длинная рокировка белых
        static constexpr uint8_t WhiteShortCastling = 4;  // Короткая рокировка белых
        static constexpr uint8_t BlackLongCastling = 5;   // Длинная рокировка чёрных
        static constexpr uint8_t BlackShortCastling = 6;  // Короткая рокировка чёрных
        
        static constexpr uint8_t PromoteToKnight = 7;     // Превращение в коня
        static constexpr uint8_t PromoteToBishop = 8;     // Превращение в слона
        static constexpr uint8_t PromoteToRook = 9;       // Превращение в ладью
        static constexpr uint8_t PromoteToQueen = 10;     // Превращение в ферзя
    };
    
    Move() = default;
    
    Move(uint8_t f, uint8_t t, uint8_t at_type, uint8_t at_side,
         uint8_t df_type, uint8_t df_side, uint8_t fl = Flag::Default)
        : from(f), to(t), attacker_type(at_type), attacker_side(at_side),
          defender_type(df_type), defender_side(df_side), flag(fl) {}
    
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to && flag == other.flag;
    }
    
    std::string to_string() const {
        std::string result;
        result += char('a' + (from % 8));
        result += char('1' + (from / 8));
        result += char('a' + (to % 8));
        result += char('1' + (to / 8));
        return result;
    }
};