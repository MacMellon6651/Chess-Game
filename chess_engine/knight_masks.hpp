#pragma once
#include "bitboard.hpp"

// Маски ходов коня для всех клеток
namespace KnightMasks {
    static constexpr std::array<Bitboard, 64> calc_masks() {
        std::array<Bitboard, 64> masks{};
        for (int x0 = 0; x0 < 8; ++x0) {
            for (int y0 = 0; y0 < 8; ++y0) {
                int p0 = y0 * 8 + x0;
                for (int x1 = 0; x1 < 8; ++x1) {
                    for (int y1 = 0; y1 < 8; ++y1) {
                        int dx = x0 > x1 ? x0 - x1 : x1 - x0;
                        int dy = y0 > y1 ? y0 - y1 : y1 - y0;
                        // Конь ходит буквой "Г"
                        if ((dx == 2 && dy == 1) || (dx == 1 && dy == 2)) {
                            BitboardOps::set_1(masks[p0], y1 * 8 + x1);
                        }
                    }
                }
            }
        }
        return masks;
    }
    static constexpr std::array<Bitboard, 64> Masks = calc_masks();
}

// Маски ходов короля для всех клеток
namespace KingMasks {
    static constexpr std::array<Bitboard, 64> calc_masks() {
        std::array<Bitboard, 64> masks{};
        for (int x0 = 0; x0 < 8; ++x0) {
            for (int y0 = 0; y0 < 8; ++y0) {
                int p0 = y0 * 8 + x0;
                for (int x1 = 0; x1 < 8; ++x1) {
                    for (int y1 = 0; y1 < 8; ++y1) {
                        int dx = x0 > x1 ? x0 - x1 : x1 - x0;
                        int dy = y0 > y1 ? y0 - y1 : y1 - y0;
                        // Король ходит на одну клетку в любом направлении
                        if (dx <= 1 && dy <= 1 && !(dx == 0 && dy == 0)) {
                            BitboardOps::set_1(masks[p0], y1 * 8 + x1);
                        }
                    }
                }
            }
        }
        return masks;
    }
    static constexpr std::array<Bitboard, 64> Masks = calc_masks();
}