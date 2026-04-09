#pragma once
#include <cstdint>
#include <array>
#include "pieces.hpp"

// Zobrist-хеширование для быстрого сравнения позиций
namespace Zobrist {
    // Генератор случайных чисел для времени компиляции
    namespace PRNG {
        static constexpr uint64_t Seed = 0x98f107ULL;
        static constexpr uint64_t Multiplier = 0x71abc9ULL;
        static constexpr uint64_t Summand = 0xff1b3fULL;
    }
    
    static constexpr uint64_t next_random(uint64_t prev) {
        return PRNG::Multiplier * prev + PRNG::Summand;
    }
    
    // 64 клетки * 2 цвета * 6 типов фигур = 768 констант
    static constexpr std::array<std::array<std::array<uint64_t, 6>, 2>, 64> calc_constants() {
        std::array<std::array<std::array<uint64_t, 6>, 2>, 64> constants{};
        uint64_t prev = PRNG::Seed;
        
        for (int square = 0; square < 64; ++square) {
            for (int side = 0; side < 2; ++side) {
                for (int type = 0; type < 6; ++type) {
                    prev = next_random(prev);
                    constants[square][side][type] = prev;
                }
            }
        }
        return constants;
    }
    
    static constexpr auto Constants = calc_constants();
    
    // Дополнительные константы для состояния игры
    static constexpr uint64_t BlackMove = next_random(Constants[63][1][5]);
    static constexpr uint64_t WhiteLongCastling = next_random(BlackMove);
    static constexpr uint64_t WhiteShortCastling = next_random(WhiteLongCastling);
    static constexpr uint64_t BlackLongCastling = next_random(WhiteShortCastling);
    static constexpr uint64_t BlackShortCastling = next_random(BlackLongCastling);
}

// Класс для работы с Zobrist-хешем
class ZobristHash {
public:
    ZobristHash() : _hash(0) {}
    
    ZobristHash(const Pieces& pieces, bool black_move, 
                bool w_l_castling, bool w_s_castling,
                bool b_l_castling, bool b_s_castling);
    
    // Инвертирование (добавление/удаление) фигуры в хеше
    void invert_piece(uint8_t square, uint8_t type, uint8_t side);
    
    // Инвертирование флагов состояния
    void invert_move();
    void invert_w_l_castling();
    void invert_w_s_castling();
    void invert_b_l_castling();
    void invert_b_s_castling();
    
    uint64_t value() const { return _hash; }
    
    bool operator==(const ZobristHash& other) const { return _hash == other._hash; }
    bool operator<(const ZobristHash& other) const { return _hash < other._hash; }
    
private:
    uint64_t _hash{0};
};