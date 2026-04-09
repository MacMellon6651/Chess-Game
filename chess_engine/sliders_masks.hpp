#pragma once
#include "bitboard.hpp"

// Маски лучей для скользящих фигур (слоны, ладьи, ферзи)
namespace SlidersMasks {
    // Направления движения
    struct Direction {
        static constexpr int8_t North = 0;
        static constexpr int8_t South = 1;
        static constexpr int8_t West = 2;
        static constexpr int8_t East = 3;
        static constexpr int8_t NorthWest = 4;
        static constexpr int8_t NorthEast = 5;
        static constexpr int8_t SouthWest = 6;
        static constexpr int8_t SouthEast = 7;
    };
    
    // Вычисление луча в заданном направлении от клетки
    static constexpr Bitboard calc_ray(uint8_t square, int8_t direction) {
        Bitboard mask = 0;
        
        int8_t x = square % 8;
        int8_t y = square / 8;
        
        while (true) {
            switch (direction) {
                case Direction::North:     y++; break;
                case Direction::South:     y--; break;
                case Direction::West:      x--; break;
                case Direction::East:      x++; break;
                case Direction::NorthWest: y++; x--; break;
                case Direction::NorthEast: y++; x++; break;
                case Direction::SouthWest: y--; x--; break;
                case Direction::SouthEast: y--; x++; break;
                default: break;
            }
            
            if (x > 7 || x < 0 || y > 7 || y < 0) break;
            BitboardOps::set_1(mask, y * 8 + x);
        }
        
        return mask;
    }
    
    // Вычисление всех лучей для всех клеток
    static constexpr std::array<std::array<Bitboard, 8>, 64> calc_masks() {
        std::array<std::array<Bitboard, 8>, 64> masks{};
        
        for (uint8_t i = 0; i < 64; ++i) {
            for (uint8_t j = 0; j < 8; ++j) {
                masks[i][j] = calc_ray(i, j);
            }
        }
        
        return masks;
    }
    
    static constexpr std::array<std::array<Bitboard, 8>, 64> Masks = calc_masks();
}