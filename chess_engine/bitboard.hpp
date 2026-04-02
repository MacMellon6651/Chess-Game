#pragma once

#include <cstdint>
#include <array>

using Bitboard = uint64_t;

namespace BitboardOps {

    // Установка бит в 1
    static constexpr void set_1(Bitboard& bb ,  uint8_t square){
        bb |= (1Ull << square);
    }

    // уСТАНОВКА БИТ В 0
    static constexpr void set_0(Bitboard& bb, uint8_t square){
        bb &= ~(1ULL << square);
    }

    // Проверить бит
    static constexpr bool get_bit(Bitboard bb, uint8_t square){
        return (bb >> square) & 1;
    }

    // кол-во единичных битов
    static constexpr int popcount(Bitboard bb){
        int count = 0;
        while (bb){
            bb &= (bb-1);
            ++count;
        }
        return count;
    }



    // Таблица для быстрой поиска по индексу (бита)
    static constexpr std::array<uint8_t, 64> BitScanTable = {
        0, 47,  1, 56, 48, 27,  2, 60,
        57, 49, 41, 37, 28, 16,  3, 61,
        54, 58, 35, 52, 50, 42, 21, 44,
        38, 32, 29, 23, 17, 11,  4, 62,
        46, 55, 26, 59, 40, 36, 15, 53,
        34, 51, 20, 43, 31, 22, 10, 45,
        25, 39, 14, 33, 19, 30,  9, 24,
        13, 18,  8, 12,  7,  6,  5, 63
    };

    // поиск младшего единичного бита
    static constexpr uint8_t bsf(Bitboard bb){
        return BitScanTable[((bb ^ (bb-1))* 0x03f79d71b4cb0a89ULL)>> 58];
    }

    // поиск старшего единичного бита
    static constexpr uint8_t bsr(Bitboard bb){
        bb |= (bb >> 1);
        bb |= (bb >> 2);
        bb |= (bb >> 4);
        bb |= (bb >> 8);
        bb |= (bb >> 16);
        bb |= (bb >> 32);

        return BitScanTable[(bb* 0x03f79d71b4cb0a89ULL)>> 58];
    }
}

namespace BitboardRows {
    static constexpr std::array<Bitboard, 8> calc_rows(){
        std::array<Bitboard, 8> rows{};

        for (uint8_t y = 0; y < 8; ++y){
            for (uint8_t x = 0; x < 8; ++x){
                BitboardOps::set_1(rows[y], y * 8 + x);
            }
        }

        return rows;
    }


    static constexpr std::array<Bitboard, 8> Rows = calc_rows();

    static constexpr std::array<Bitboard, 8> calc_innversion_rows(){
        std::array<Bitboard, 8> rows{};
        for (uint8_t i = 0 ; i < 8 ; ++i) {
            rows[i] =  ~Rows[i];
        }
        return rows;
    }
    static constexpr std::array<Bitboard, 8> Inversion = calc_innversion_rows();

}

namespace BitboardColumns{
    
    static constexpr std::array<Bitboard, 8> calc_columns(){
        std::array<Bitboard, 8> columns{};
        for (uint8_t x = 0; x < 8; ++x){
            for (uint8_t y = 0; y < 8; ++y){
                BitboardOps::set_1(columns[x], y * 8 + x);
            }
        }
        return columns;
    }

    static constexpr std::array<Bitboard, 8> Columns = calc_columns();

    static constexpr std::array<Bitboard, 8> calc_inversion_columns(){
        std::array<Bitboard, 8> columns{};
        for (uint8_t i = 0; i < 8; ++i) columns[i] = ~Columns[i];
        return columns;
    }

    static constexpr std::array<Bitboard, 8> InversionColumns =  calc_inversion_columns();
} 

