#include "zobrist.hpp"

ZobristHash::ZobristHash(const Pieces& pieces, bool black_move,
                         bool w_l_castling, bool w_s_castling,
                         bool b_l_castling, bool b_s_castling) {
    _hash = 0;
    
    // Добавляем флаги состояния
    if (black_move) invert_move();
    if (w_l_castling) invert_w_l_castling();
    if (w_s_castling) invert_w_s_castling();
    if (b_l_castling) invert_b_l_castling();
    if (b_s_castling) invert_b_s_castling();
    
    // Добавляем все фигуры
    for (uint8_t square = 0; square < 64; ++square) {
        uint8_t color = pieces.color_at(square);
        if (color == 255) continue;  // пусто
        
        uint8_t type = pieces.piece_at(square);
        invert_piece(square, type, color);
    }
}

void ZobristHash::invert_piece(uint8_t square, uint8_t type, uint8_t side) {
    _hash ^= Zobrist::Constants[square][side][type];
}

void ZobristHash::invert_move() {
    _hash ^= Zobrist::BlackMove;
}

void ZobristHash::invert_w_l_castling() {
    _hash ^= Zobrist::WhiteLongCastling;
}

void ZobristHash::invert_w_s_castling() {
    _hash ^= Zobrist::WhiteShortCastling;
}

void ZobristHash::invert_b_l_castling() {
    _hash ^= Zobrist::BlackLongCastling;
}

void ZobristHash::invert_b_s_castling() {
    _hash ^= Zobrist::BlackShortCastling;
}