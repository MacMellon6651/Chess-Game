#pragma once
#include "pieces.hpp"
#include "move_list.hpp"
#include "move.hpp"
#include "knight_masks.hpp"
#include "sliders_masks.hpp"

// Генератор легальных шахматных ходов
namespace MoveGen {
    // Генерация всех легальных ходов для стороны
    MoveList generate_all_moves(const Pieces& pieces, uint8_t side, 
                                uint8_t en_passant = 255,
                                bool w_l_castling = true, bool w_s_castling = true,
                                bool b_l_castling = true, bool b_s_castling = true);
    
    // Проверка, находится ли король под шахом
    bool is_check(const Pieces& pieces, uint8_t side);
    
    // Проверка, есть ли хотя бы один легальный ход
    bool has_legal_moves(const Pieces& pieces, uint8_t side,
                         uint8_t en_passant = 255,
                         bool w_l_castling = true, bool w_s_castling = true,
                         bool b_l_castling = true, bool b_s_castling = true);
    
    // Проверка конкретного хода на легальность
    bool is_legal_move(const Pieces& pieces, const Move& move, 
                       uint8_t en_passant = 255,
                       bool w_l_castling = true, bool w_s_castling = true,
                       bool b_l_castling = true, bool b_s_castling = true);
    
    // Проверка, атакована ли клетка фигурами указанной стороны
    bool is_square_attacked(const Pieces& pieces, uint8_t square, uint8_t by_side);
}