#include "move_gen.hpp"
#include "sliders_masks.hpp"
#include <cmath>

namespace MoveGen {

// Проверка атаки пешками
static bool _is_square_attacked_by_pawns(const Pieces& pieces, uint8_t square, uint8_t by_side) {
    if (by_side == PieceColor::White) {
        // Белые пешки атакуют по диагонали вверх
        if (square % 8 != 0 && square >= 8) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Pawn], square - 9))
                return true;
        }
        if (square % 8 != 7 && square >= 8) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Pawn], square - 7))
                return true;
        }
    } else {
        // Чёрные пешки атакуют по диагонали вниз
        if (square % 8 != 0 && square <= 55) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Pawn], square + 7))
                return true;
        }
        if (square % 8 != 7 && square <= 55) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Pawn], square + 9))
                return true;
        }
    }
    return false;
}

// Проверка атаки конями
static bool _is_square_attacked_by_knights(const Pieces& pieces, uint8_t square, uint8_t by_side) {
    Bitboard attacks = KnightMasks::Masks[square];
    return (attacks & pieces.piece_bitboards[by_side][PieceType::Knight]) != 0;
}

// Проверка атаки королём
static bool _is_square_attacked_by_king(const Pieces& pieces, uint8_t square, uint8_t by_side) {
    Bitboard attacks = KingMasks::Masks[square];
    return (attacks & pieces.piece_bitboards[by_side][PieceType::King]) != 0;
}

// Вычисление луча с учётом блокирующих фигур
static Bitboard _calc_ray(const Pieces& pieces, uint8_t p, uint8_t side, 
                           bool only_captures, int8_t direction, bool bsr_flag) {
    Bitboard ray = SlidersMasks::Masks[p][direction];
    Bitboard blockers = ray & pieces.all;
    
    if (blockers == 0) {
        if (only_captures) return 0;
        return ray;
    }
    
    uint8_t blocking_square;
    if (bsr_flag) blocking_square = BitboardOps::bsr(blockers);
    else blocking_square = BitboardOps::bsf(blockers);
    
    Bitboard moves;
    if (only_captures) moves = 0;
    else moves = ray ^ SlidersMasks::Masks[blocking_square][direction];
    
    if (BitboardOps::get_bit(pieces.side_bitboards[PieceColor::inverse(side)], blocking_square)) {
        BitboardOps::set_1(moves, blocking_square);
    } else {
        BitboardOps::set_0(moves, blocking_square);
    }
    
    return moves;
}

// Маска ходов слона
static Bitboard _generate_bishop_mask(const Pieces& pieces, uint8_t p, uint8_t side, bool only_captures) {
    Bitboard nw = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::NorthWest, false);
    Bitboard ne = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::NorthEast, false);
    Bitboard sw = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::SouthWest, true);
    Bitboard se = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::SouthEast, true);
    return nw | ne | sw | se;
}

// Маска ходов ладьи
static Bitboard _generate_rook_mask(const Pieces& pieces, uint8_t p, uint8_t side, bool only_captures) {
    Bitboard n = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::North, false);
    Bitboard s = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::South, true);
    Bitboard w = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::West, true);
    Bitboard e = _calc_ray(pieces, p, side, only_captures, SlidersMasks::Direction::East, false);
    return n | s | w | e;
}

// Маска ходов ферзя
static Bitboard _generate_queen_mask(const Pieces& pieces, uint8_t p, uint8_t side, bool only_captures) {
    return _generate_bishop_mask(pieces, p, side, only_captures) |
           _generate_rook_mask(pieces, p, side, only_captures);
}

// Публичная функция: проверка атаки клетки
bool is_square_attacked(const Pieces& pieces, uint8_t square, uint8_t by_side) {
    if (_is_square_attacked_by_pawns(pieces, square, by_side)) return true;
    if (_is_square_attacked_by_knights(pieces, square, by_side)) return true;
    if (_is_square_attacked_by_king(pieces, square, by_side)) return true;
    
    Bitboard bishop_attacks = _generate_bishop_mask(pieces, square, by_side, true);
    if (bishop_attacks & (pieces.piece_bitboards[by_side][PieceType::Bishop] |
                          pieces.piece_bitboards[by_side][PieceType::Queen])) return true;
    
    Bitboard rook_attacks = _generate_rook_mask(pieces, square, by_side, true);
    if (rook_attacks & (pieces.piece_bitboards[by_side][PieceType::Rook] |
                        pieces.piece_bitboards[by_side][PieceType::Queen])) return true;
    
    return false;
}

// Публичная функция: проверка шаха
bool is_check(const Pieces& pieces, uint8_t side) {
    Bitboard king_bb = pieces.piece_bitboards[side][PieceType::King];
    if (king_bb == 0) return false;
    
    uint8_t king_square = BitboardOps::bsf(king_bb);
    return is_square_attacked(pieces, king_square, PieceColor::inverse(side));
}

// Проверка легальности хода (применение на копии)
static bool _is_legal_after_move(Pieces pieces, const Move& move, bool en_passant_capture) {
    // Применяем ход
    BitboardOps::set_0(pieces.piece_bitboards[move.attacker_side][move.attacker_type], move.from);
    BitboardOps::set_1(pieces.piece_bitboards[move.attacker_side][move.attacker_type], move.to);
    
    if (move.defender_type != 255) {
        BitboardOps::set_0(pieces.piece_bitboards[move.defender_side][move.defender_type], move.to);
    }
    
    if (en_passant_capture) {
        if (move.attacker_side == PieceColor::White) {
            BitboardOps::set_0(pieces.piece_bitboards[PieceColor::Black][PieceType::Pawn], move.to - 8);
        } else {
            BitboardOps::set_0(pieces.piece_bitboards[PieceColor::White][PieceType::Pawn], move.to + 8);
        }
    }
    
    pieces.update_bitboards();
    
    Bitboard king_bb = pieces.piece_bitboards[move.attacker_side][PieceType::King];
    uint8_t king_square = BitboardOps::bsf(king_bb);
    
    return !is_square_attacked(pieces, king_square, PieceColor::inverse(move.attacker_side));
}

// Публичная функция: проверка легальности хода
bool is_legal_move(const Pieces& pieces, const Move& move, uint8_t en_passant,
                   bool w_l_castling, bool w_s_castling,
                   bool b_l_castling, bool b_s_castling) {
    
    // Обработка рокировки
    if (move.flag >= Move::Flag::WhiteLongCastling && 
        move.flag <= Move::Flag::BlackShortCastling) {
        
        uint8_t side = (move.flag == Move::Flag::WhiteLongCastling || 
                       move.flag == Move::Flag::WhiteShortCastling) ? 
                       PieceColor::White : PieceColor::Black;
        
        if (is_check(pieces, side)) return false;
        
        if (move.flag == Move::Flag::WhiteShortCastling) {
            if (!w_s_castling) return false;
            for (uint8_t sq = 5; sq <= 6; ++sq) {
                if (is_square_attacked(pieces, sq, PieceColor::Black)) return false;
            }
        } 
        else if (move.flag == Move::Flag::WhiteLongCastling) {
            if (!w_l_castling) return false;
            for (uint8_t sq = 2; sq <= 3; ++sq) {
                if (is_square_attacked(pieces, sq, PieceColor::Black)) return false;
            }
        } 
        else if (move.flag == Move::Flag::BlackShortCastling) {
            if (!b_s_castling) return false;
            for (uint8_t sq = 61; sq <= 62; ++sq) {
                if (is_square_attacked(pieces, sq, PieceColor::White)) return false;
            }
        } 
        else if (move.flag == Move::Flag::BlackLongCastling) {
            if (!b_l_castling) return false;
            for (uint8_t sq = 58; sq <= 59; ++sq) {
                if (is_square_attacked(pieces, sq, PieceColor::White)) return false;
            }
        }
        return true;
    }
    
    bool en_passant_capture = (move.flag == Move::Flag::EnPassantCapture);
    return _is_legal_after_move(pieces, move, en_passant_capture);
}

// Преобразование маски пешек в список ходов
static void _pawn_mask_to_moves(const Pieces& pieces, Bitboard mask, uint8_t attacker_side,
                                int8_t attacker_index, bool look_for_defender,
                                uint8_t flag, MoveList& moves, uint8_t en_passant,
                                bool w_l_castling, bool w_s_castling,
                                bool b_l_castling, bool b_s_castling) {
    uint8_t defender_p;
    uint8_t defender_type = 255;
    
    while (mask) {
        defender_p = BitboardOps::bsf(mask);
        BitboardOps::set_0(mask, defender_p);
        
        if (look_for_defender) {
            defender_type = 255;
            for (uint8_t i = 0; i < 6; ++i) {
                if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::inverse(attacker_side)][i], defender_p)) {
                    defender_type = i;
                    break;
                }
            }
        }
        
        Move move(static_cast<uint8_t>(defender_p + attacker_index), defender_p,
                 PieceType::Pawn, attacker_side, defender_type, 
                 PieceColor::inverse(attacker_side), flag);
        
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling, 
                         b_l_castling, b_s_castling)) {
            // Превращение на последней линии
            if (defender_p < 8 || defender_p > 55) {
                moves.push_back(Move(move.from, move.to, PieceType::Pawn, attacker_side,
                                    defender_type, PieceColor::inverse(attacker_side),
                                    Move::Flag::PromoteToKnight));
                moves.push_back(Move(move.from, move.to, PieceType::Pawn, attacker_side,
                                    defender_type, PieceColor::inverse(attacker_side),
                                    Move::Flag::PromoteToBishop));
                moves.push_back(Move(move.from, move.to, PieceType::Pawn, attacker_side,
                                    defender_type, PieceColor::inverse(attacker_side),
                                    Move::Flag::PromoteToRook));
                moves.push_back(Move(move.from, move.to, PieceType::Pawn, attacker_side,
                                    defender_type, PieceColor::inverse(attacker_side),
                                    Move::Flag::PromoteToQueen));
            } else {
                moves.push_back(move);
            }
        }
    }
}

// Преобразование маски фигур в список ходов
static void _piece_mask_to_moves(const Pieces& pieces, Bitboard mask, uint8_t attacker_p,
                                 uint8_t attacker_type, uint8_t attacker_side,
                                 MoveList& moves, uint8_t en_passant,
                                 bool w_l_castling, bool w_s_castling,
                                 bool b_l_castling, bool b_s_castling) {
    uint8_t defender_p;
    uint8_t defender_type;
    
    while (mask) {
        defender_p = BitboardOps::bsf(mask);
        BitboardOps::set_0(mask, defender_p);
        
        defender_type = 255;
        for (uint8_t i = 0; i < 6; ++i) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::inverse(attacker_side)][i], defender_p)) {
                defender_type = i;
                break;
            }
        }
        
        Move move(attacker_p, defender_p, attacker_type, attacker_side,
                 defender_type, PieceColor::inverse(attacker_side));
        
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                         b_l_castling, b_s_castling)) {
            moves.push_back(move);
        }
    }
}

// Ходы пешек
static void _add_pawn_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                            uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                            bool b_l_castling, bool b_s_castling) {
    
    Bitboard pawns = pieces.piece_bitboards[side][PieceType::Pawn];
    Bitboard empty = pieces.empty;
    
    // Обычные ходы (на 1 клетку)
    Bitboard default_mask;
    if (side == PieceColor::White) {
        default_mask = (pawns << 8) & empty;
    } else {
        default_mask = (pawns >> 8) & empty;
    }
    
    Bitboard temp = default_mask;
    while (temp) {
        uint8_t to = BitboardOps::bsf(temp);
        BitboardOps::set_0(temp, to);
        uint8_t from = (side == PieceColor::White) ? to - 8 : to + 8;
        
        Move move(from, to, PieceType::Pawn, side, 255, 255);
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                         b_l_castling, b_s_castling)) {
            if (to < 8 || to > 55) {
                moves.push_back(Move(from, to, PieceType::Pawn, side, 255, 255, Move::Flag::PromoteToKnight));
                moves.push_back(Move(from, to, PieceType::Pawn, side, 255, 255, Move::Flag::PromoteToBishop));
                moves.push_back(Move(from, to, PieceType::Pawn, side, 255, 255, Move::Flag::PromoteToRook));
                moves.push_back(Move(from, to, PieceType::Pawn, side, 255, 255, Move::Flag::PromoteToQueen));
            } else {
                moves.push_back(move);
            }
        }
    }
    
    // Длинные ходы (на 2 клетки)
    Bitboard long_mask;
    if (side == PieceColor::White) {
        long_mask = ((default_mask & BitboardRows::Rows[2]) << 8) & empty;
    } else {
        long_mask = ((default_mask & BitboardRows::Rows[5]) >> 8) & empty;
    }
    
    temp = long_mask;
    while (temp) {
        uint8_t to = BitboardOps::bsf(temp);
        BitboardOps::set_0(temp, to);
        uint8_t from = (side == PieceColor::White) ? to - 16 : to + 16;
        
        Move move(from, to, PieceType::Pawn, side, 255, 255, Move::Flag::PawnLongMove);
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                         b_l_castling, b_s_castling)) {
            moves.push_back(move);
        }
    }
    
    // Взятия влево
    Bitboard left_captures;
    if (side == PieceColor::White) {
        left_captures = (pawns << 7) & BitboardColumns::InversionColumns[7] & pieces.side_bitboards[PieceColor::Black];
    } else {
        left_captures = (pawns >> 9) & BitboardColumns::InversionColumns[7] & pieces.side_bitboards[PieceColor::White];
    }
    
    temp = left_captures;
    while (temp) {
        uint8_t to = BitboardOps::bsf(temp);
        BitboardOps::set_0(temp, to);
        uint8_t from = (side == PieceColor::White) ? to - 7 : to + 9;
        
        uint8_t defender_type = 255;
        for (uint8_t i = 0; i < 6; ++i) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::inverse(side)][i], to)) {
                defender_type = i;
                break;
            }
        }
        
        Move move(from, to, PieceType::Pawn, side, defender_type, PieceColor::inverse(side));
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                         b_l_castling, b_s_castling)) {
            if (to < 8 || to > 55) {
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type, 
                                    PieceColor::inverse(side), Move::Flag::PromoteToKnight));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToBishop));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToRook));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToQueen));
            } else {
                moves.push_back(move);
            }
        }
    }
    
    // Взятия вправо
    Bitboard right_captures;
    if (side == PieceColor::White) {
        right_captures = (pawns << 9) & BitboardColumns::InversionColumns[0] & pieces.side_bitboards[PieceColor::Black];
    } else {
        right_captures = (pawns >> 7) & BitboardColumns::InversionColumns[0] & pieces.side_bitboards[PieceColor::White];
    }
    
    temp = right_captures;
    while (temp) {
        uint8_t to = BitboardOps::bsf(temp);
        BitboardOps::set_0(temp, to);
        uint8_t from = (side == PieceColor::White) ? to - 9 : to + 7;
        
        uint8_t defender_type = 255;
        for (uint8_t i = 0; i < 6; ++i) {
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::inverse(side)][i], to)) {
                defender_type = i;
                break;
            }
        }
        
        Move move(from, to, PieceType::Pawn, side, defender_type, PieceColor::inverse(side));
        if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                         b_l_castling, b_s_castling)) {
            if (to < 8 || to > 55) {
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToKnight));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToBishop));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToRook));
                moves.push_back(Move(from, to, PieceType::Pawn, side, defender_type,
                                    PieceColor::inverse(side), Move::Flag::PromoteToQueen));
            } else {
                moves.push_back(move);
            }
        }
    }
    
    // Взятие на проходе
    if (en_passant != 255) {
        if (side == PieceColor::White && en_passant >= 16 && en_passant <= 23) {
            if (en_passant % 8 != 0 && BitboardOps::get_bit(pawns, en_passant - 9)) {
                Move move(en_passant - 9, en_passant, PieceType::Pawn, PieceColor::White,
                         255, 255, Move::Flag::EnPassantCapture);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
            if (en_passant % 8 != 7 && BitboardOps::get_bit(pawns, en_passant - 7)) {
                Move move(en_passant - 7, en_passant, PieceType::Pawn, PieceColor::White,
                         255, 255, Move::Flag::EnPassantCapture);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
        else if (side == PieceColor::Black && en_passant >= 40 && en_passant <= 47) {
            if (en_passant % 8 != 0 && BitboardOps::get_bit(pawns, en_passant + 7)) {
                Move move(en_passant + 7, en_passant, PieceType::Pawn, PieceColor::Black,
                         255, 255, Move::Flag::EnPassantCapture);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
            if (en_passant % 8 != 7 && BitboardOps::get_bit(pawns, en_passant + 9)) {
                Move move(en_passant + 9, en_passant, PieceType::Pawn, PieceColor::Black,
                         255, 255, Move::Flag::EnPassantCapture);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
    }
}

// Ходы коней
static void _add_knight_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                              uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                              bool b_l_castling, bool b_s_castling) {
    Bitboard knights = pieces.piece_bitboards[side][PieceType::Knight];
    
    while (knights) {
        uint8_t from = BitboardOps::bsf(knights);
        BitboardOps::set_0(knights, from);
        
        Bitboard mask = KnightMasks::Masks[from] & pieces.inversion_side_bitboards[side];
        _piece_mask_to_moves(pieces, mask, from, PieceType::Knight, side, moves,
                            en_passant, w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    }
}

// Ходы слонов
static void _add_bishop_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                              uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                              bool b_l_castling, bool b_s_castling) {
    Bitboard bishops = pieces.piece_bitboards[side][PieceType::Bishop];
    
    while (bishops) {
        uint8_t from = BitboardOps::bsf(bishops);
        BitboardOps::set_0(bishops, from);
        
        Bitboard mask = _generate_bishop_mask(pieces, from, side, false);
        _piece_mask_to_moves(pieces, mask, from, PieceType::Bishop, side, moves,
                            en_passant, w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    }
}

// Ходы ладей
static void _add_rook_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                            uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                            bool b_l_castling, bool b_s_castling) {
    Bitboard rooks = pieces.piece_bitboards[side][PieceType::Rook];
    
    while (rooks) {
        uint8_t from = BitboardOps::bsf(rooks);
        BitboardOps::set_0(rooks, from);
        
        Bitboard mask = _generate_rook_mask(pieces, from, side, false);
        _piece_mask_to_moves(pieces, mask, from, PieceType::Rook, side, moves,
                            en_passant, w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    }
}

// Ходы ферзей
static void _add_queen_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                             uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                             bool b_l_castling, bool b_s_castling) {
    Bitboard queens = pieces.piece_bitboards[side][PieceType::Queen];
    
    while (queens) {
        uint8_t from = BitboardOps::bsf(queens);
        BitboardOps::set_0(queens, from);
        
        Bitboard mask = _generate_queen_mask(pieces, from, side, false);
        _piece_mask_to_moves(pieces, mask, from, PieceType::Queen, side, moves,
                            en_passant, w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    }
}

// Ходы короля (обычные перемещения)
static void _add_king_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                            uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                            bool b_l_castling, bool b_s_castling) {
    Bitboard kings = pieces.piece_bitboards[side][PieceType::King];
    
    while (kings) {
        uint8_t from = BitboardOps::bsf(kings);
        BitboardOps::set_0(kings, from);
        
        Bitboard mask = KingMasks::Masks[from] & pieces.inversion_side_bitboards[side];
        _piece_mask_to_moves(pieces, mask, from, PieceType::King, side, moves,
                            en_passant, w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    }
}

// Рокировки
static void _add_castling_moves(const Pieces& pieces, uint8_t side, MoveList& moves,
                                uint8_t en_passant, bool w_l_castling, bool w_s_castling,
                                bool b_l_castling, bool b_s_castling) {
    
    if (side == PieceColor::White) {
        // Короткая рокировка белых
        if (w_s_castling) {
            bool empty_between = true;
            for (uint8_t sq = 5; sq <= 6; ++sq) {
                if (!BitboardOps::get_bit(pieces.empty, sq)) {
                    empty_between = false;
                    break;
                }
            }
            if (empty_between) {
                Move move(4, 6, PieceType::King, PieceColor::White,
                         255, 255, Move::Flag::WhiteShortCastling);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
        
        // Длинная рокировка белых
        if (w_l_castling) {
            bool empty_between = true;
            for (uint8_t sq = 1; sq <= 3; ++sq) {
                if (!BitboardOps::get_bit(pieces.empty, sq)) {
                    empty_between = false;
                    break;
                }
            }
            if (empty_between) {
                Move move(4, 2, PieceType::King, PieceColor::White,
                         255, 255, Move::Flag::WhiteLongCastling);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
    } else {
        // Короткая рокировка чёрных
        if (b_s_castling) {
            bool empty_between = true;
            for (uint8_t sq = 61; sq <= 62; ++sq) {
                if (!BitboardOps::get_bit(pieces.empty, sq)) {
                    empty_between = false;
                    break;
                }
            }
            if (empty_between) {
                Move move(60, 62, PieceType::King, PieceColor::Black,
                         255, 255, Move::Flag::BlackShortCastling);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
        
        // Длинная рокировка чёрных
        if (b_l_castling) {
            bool empty_between = true;
            for (uint8_t sq = 57; sq <= 59; ++sq) {
                if (!BitboardOps::get_bit(pieces.empty, sq)) {
                    empty_between = false;
                    break;
                }
            }
            if (empty_between) {
                Move move(60, 58, PieceType::King, PieceColor::Black,
                         255, 255, Move::Flag::BlackLongCastling);
                if (is_legal_move(pieces, move, en_passant, w_l_castling, w_s_castling,
                                 b_l_castling, b_s_castling)) {
                    moves.push_back(move);
                }
            }
        }
    }
}

// Публичная функция: генерация всех легальных ходов
MoveList generate_all_moves(const Pieces& pieces, uint8_t side, uint8_t en_passant,
                            bool w_l_castling, bool w_s_castling,
                            bool b_l_castling, bool b_s_castling) {
    MoveList moves;
    
    _add_pawn_moves(pieces, side, moves, en_passant, 
                   w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_knight_moves(pieces, side, moves, en_passant,
                     w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_bishop_moves(pieces, side, moves, en_passant,
                     w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_rook_moves(pieces, side, moves, en_passant,
                   w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_queen_moves(pieces, side, moves, en_passant,
                    w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_king_moves(pieces, side, moves, en_passant,
                   w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    _add_castling_moves(pieces, side, moves, en_passant,
                       w_l_castling, w_s_castling, b_l_castling, b_s_castling);
    
    return moves;
}

// Публичная функция: проверка наличия легальных ходов
bool has_legal_moves(const Pieces& pieces, uint8_t side, uint8_t en_passant,
                     bool w_l_castling, bool w_s_castling,
                     bool b_l_castling, bool b_s_castling) {
    MoveList moves = generate_all_moves(pieces, side, en_passant,
                                        w_l_castling, w_s_castling,
                                        b_l_castling, b_s_castling);
    return moves.size() > 0;
}

} // namespace MoveGen