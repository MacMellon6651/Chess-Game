#pragma once 

#include "bitboard.hpp"
#include <string>
#include <cctype>
#include <ostream>

namespace PieceType{

    static constexpr uint8_t Pawn = 0;
    static constexpr uint8_t Knight = 1;
    static constexpr uint8_t Bishop = 2;
    static constexpr uint8_t Rook = 3;
    static constexpr uint8_t Queen = 4;
    static constexpr uint8_t King = 5;

    static constexpr const char* to_char(uint8_t type, uint8_t color){
        const char pieces[2][6] = {
            {'P','N','B','R','Q','K'},
            {'p','n','b','r','q','k'}
        };
        return &pieces[color][type];
    }
}

namespace PieceColor {
    static constexpr uint8_t White = 0;
    static constexpr uint8_t Black = 1;
    
    
    static constexpr uint8_t inverse(uint8_t side){
        return side ^ 1;
    }
}


struct Pieces {

    std::array<std::array<Bitboard, 6>, 2> piece_bitboards{};
}