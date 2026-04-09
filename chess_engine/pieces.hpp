#pragma once

#include "bitboard.hpp"
#include <string>
#include <cctype>
#include <ostream>

// Типы фигур
namespace PieceType {
    static constexpr uint8_t Pawn   = 0;
    static constexpr uint8_t Knight = 1;
    static constexpr uint8_t Bishop = 2;
    static constexpr uint8_t Rook   = 3;
    static constexpr uint8_t Queen  = 4;
    static constexpr uint8_t King   = 5;

    static constexpr const char* to_char(uint8_t type, uint8_t color) {
        const char pieces[2][6] = {
            {'P', 'N', 'B', 'R', 'Q', 'K'},  // Белые
            {'p', 'n', 'b', 'r', 'q', 'k'}   // Чёрные
        };
        return &pieces[color][type];
    }
}

// Цвета фигур
namespace PieceColor {
    static constexpr uint8_t White = 0;
    static constexpr uint8_t Black = 1;
    
    // Инверсия цвета (белый -> чёрный, чёрный -> белый)
    static constexpr uint8_t inverse(uint8_t side) {
        return side ^ 1;
    }
}

// Структура для хранения всех фигур на доске (12 битбордов)
struct Pieces {
    // Основные битборды: [цвет][тип фигуры]
    std::array<std::array<Bitboard, 6>, 2> piece_bitboards{};

    // Вспомогательные битборды
    std::array<Bitboard, 2> side_bitboards{};              // Все фигуры цвета
    std::array<Bitboard, 2> inversion_side_bitboards{};    // ~side_bitboards
    Bitboard all{};   // Все фигуры на доске
    Bitboard empty{}; // Пустые клетки

    // Конструктор из FEN-строки (только часть с фигурами)
    explicit Pieces(const std::string& fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

    // Обновление вспомогательных битбордов на основе основных
    void update_bitboards();

    // Сравнение двух позиций
    bool operator==(const Pieces& other) const;

    // Вывод доски в консоль (с символами Юникода)
    friend std::ostream& operator<<(std::ostream& os, const Pieces& pieces);

    // Получить тип фигуры на клетке (255 если пусто)
    uint8_t piece_at(uint8_t square) const;
    
    // Получить цвет фигуры на клетке (255 если пусто)
    uint8_t color_at(uint8_t square) const;
};