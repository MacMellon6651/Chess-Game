#include "position.hpp"
#include "move_gen.hpp"
#include <cmath>

Position::Position() 
    : _pieces("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR")
    , _en_passant(255)
    , _w_l_castling(true), _w_s_castling(true)
    , _b_l_castling(true), _b_s_castling(true)
    , _move_ctr(0.0f)
    , _fifty_moves_ctr(0.0f) {
    _hash = ZobristHash(_pieces, false, true, true, true, true);
    _history.push_back(_hash);
}

Position::Position(const std::string& fen) {
    // Извлекаем только часть с фигурами (до первого пробела)
    std::string board_part = fen;
    size_t space_pos = fen.find(' ');
    if (space_pos != std::string::npos) {
        board_part = fen.substr(0, space_pos);
    }
    
    _pieces = Pieces(board_part);
    _en_passant = 255;
    _w_l_castling = true;
    _w_s_castling = true;
    _b_l_castling = true;
    _b_s_castling = true;
    _move_ctr = 0.0f;
    _fifty_moves_ctr = 0.0f;
    _hash = ZobristHash(_pieces, false, true, true, true, true);
    _history.push_back(_hash);
}

std::string Position::get_fen() const {
    std::string fen;
    
    // Расстановка фигур
    for (int y = 7; y >= 0; --y) {
        int empty = 0;
        for (int x = 0; x < 8; ++x) {
            uint8_t square = y * 8 + x;
            uint8_t color = _pieces.color_at(square);
            uint8_t type = _pieces.piece_at(square);
            
            if (color == 255) {
                empty++;
            } else {
                if (empty > 0) {
                    fen += std::to_string(empty);
                    empty = 0;
                }
                char piece_char;
                if (color == PieceColor::White) {
                    switch (type) {
                        case PieceType::Pawn:   piece_char = 'P'; break;
                        case PieceType::Knight: piece_char = 'N'; break;
                        case PieceType::Bishop: piece_char = 'B'; break;
                        case PieceType::Rook:   piece_char = 'R'; break;
                        case PieceType::Queen:  piece_char = 'Q'; break;
                        case PieceType::King:   piece_char = 'K'; break;
                        default: piece_char = '?'; break;
                    }
                } else {
                    switch (type) {
                        case PieceType::Pawn:   piece_char = 'p'; break;
                        case PieceType::Knight: piece_char = 'n'; break;
                        case PieceType::Bishop: piece_char = 'b'; break;
                        case PieceType::Rook:   piece_char = 'r'; break;
                        case PieceType::Queen:  piece_char = 'q'; break;
                        case PieceType::King:   piece_char = 'k'; break;
                        default: piece_char = '?'; break;
                    }
                }
                fen += piece_char;
            }
        }
        if (empty > 0) fen += std::to_string(empty);
        if (y > 0) fen += '/';
    }
    
    // Очерёдность хода
    fen += ' ';
    fen += (side_to_move() == PieceColor::White) ? 'w' : 'b';
    
    // Права на рокировку
    fen += ' ';
    if (!_w_l_castling && !_w_s_castling && !_b_l_castling && !_b_s_castling) {
        fen += '-';
    } else {
        if (_w_s_castling) fen += 'K';
        if (_w_l_castling) fen += 'Q';
        if (_b_s_castling) fen += 'k';
        if (_b_l_castling) fen += 'q';
    }
    
    // Битое поле
    fen += ' ';
    if (_en_passant == 255) {
        fen += '-';
    } else {
        int file = _en_passant % 8;
        int rank = _en_passant / 8;
        fen += char('a' + file);
        fen += char('1' + rank);
    }
    
    // Счётчик 50 ходов
    fen += ' ';
    fen += std::to_string(static_cast<int>(_fifty_moves_ctr));
    
    // Номер хода
    fen += ' ';
    fen += std::to_string(static_cast<int>(_move_ctr) + 1);
    
    return fen;
}

void Position::_add_piece(uint8_t square, uint8_t type, uint8_t side) {
    if (!BitboardOps::get_bit(_pieces.piece_bitboards[side][type], square)) {
        BitboardOps::set_1(_pieces.piece_bitboards[side][type], square);
        _hash.invert_piece(square, type, side);
    }
}

void Position::_remove_piece(uint8_t square, uint8_t type, uint8_t side) {
    if (BitboardOps::get_bit(_pieces.piece_bitboards[side][type], square)) {
        BitboardOps::set_0(_pieces.piece_bitboards[side][type], square);
        _hash.invert_piece(square, type, side);
    }
}

void Position::_update_castling_rights(uint8_t from) {
    // Белые
    if (from == 0) {
        _hash.invert_w_l_castling();
        _w_l_castling = false;
    }
    if (from == 4) {
        _hash.invert_w_l_castling();
        _w_l_castling = false;
        _hash.invert_w_s_castling();
        _w_s_castling = false;
    }
    if (from == 7) {
        _hash.invert_w_s_castling();
        _w_s_castling = false;
    }
    
    // Чёрные
    if (from == 56) {
        _hash.invert_b_l_castling();
        _b_l_castling = false;
    }
    if (from == 60) {
        _hash.invert_b_l_castling();
        _b_l_castling = false;
        _hash.invert_b_s_castling();
        _b_s_castling = false;
    }
    if (from == 63) {
        _hash.invert_b_s_castling();
        _b_s_castling = false;
    }
}

bool Position::apply_move(const Move& move) {
    // Сохраняем состояние для возможного отката
    Pieces old_pieces = _pieces;
    uint8_t old_en_passant = _en_passant;
    bool old_w_l = _w_l_castling, old_w_s = _w_s_castling;
    bool old_b_l = _b_l_castling, old_b_s = _b_s_castling;
    float old_fifty = _fifty_moves_ctr;
    
    // Перемещаем фигуру
    _remove_piece(move.from, move.attacker_type, move.attacker_side);
    _add_piece(move.to, move.attacker_type, move.attacker_side);
    
    // Удаляем взятую фигуру
    if (move.defender_type != 255) {
        _remove_piece(move.to, move.defender_type, move.defender_side);
    }
    
    // Обработка специальных флагов
    switch (move.flag) {
        case Move::Flag::EnPassantCapture:
            if (move.attacker_side == PieceColor::White)
                _remove_piece(move.to - 8, PieceType::Pawn, PieceColor::Black);
            else
                _remove_piece(move.to + 8, PieceType::Pawn, PieceColor::White);
            break;
            
        case Move::Flag::WhiteLongCastling:
            _remove_piece(0, PieceType::Rook, PieceColor::White);
            _add_piece(3, PieceType::Rook, PieceColor::White);
            break;
        case Move::Flag::WhiteShortCastling:
            _remove_piece(7, PieceType::Rook, PieceColor::White);
            _add_piece(5, PieceType::Rook, PieceColor::White);
            break;
        case Move::Flag::BlackLongCastling:
            _remove_piece(56, PieceType::Rook, PieceColor::Black);
            _add_piece(59, PieceType::Rook, PieceColor::Black);
            break;
        case Move::Flag::BlackShortCastling:
            _remove_piece(63, PieceType::Rook, PieceColor::Black);
            _add_piece(61, PieceType::Rook, PieceColor::Black);
            break;
            
        case Move::Flag::PromoteToKnight:
            _remove_piece(move.to, PieceType::Pawn, move.attacker_side);
            _add_piece(move.to, PieceType::Knight, move.attacker_side);
            break;
        case Move::Flag::PromoteToBishop:
            _remove_piece(move.to, PieceType::Pawn, move.attacker_side);
            _add_piece(move.to, PieceType::Bishop, move.attacker_side);
            break;
        case Move::Flag::PromoteToRook:
            _remove_piece(move.to, PieceType::Pawn, move.attacker_side);
            _add_piece(move.to, PieceType::Rook, move.attacker_side);
            break;
        case Move::Flag::PromoteToQueen:
            _remove_piece(move.to, PieceType::Pawn, move.attacker_side);
            _add_piece(move.to, PieceType::Queen, move.attacker_side);
            break;
            
        default: break;
    }
    
    // Обновляем вспомогательные битборды
    _pieces.update_bitboards();
    
    // Обновляем права на рокировку
    _update_castling_rights(move.from);
    
    // Обновляем en passant
    if (move.flag != Move::Flag::PawnLongMove) {
        if (_en_passant != 255) {
            _hash.invert_piece(_en_passant, PieceType::Pawn, PieceColor::inverse(side_to_move()));
            _en_passant = 255;
        }
    } else {
        _en_passant = (move.from + move.to) / 2;
        _hash.invert_piece(_en_passant, PieceType::Pawn, PieceColor::inverse(side_to_move()));
    }
    
    // Обновляем счётчик ходов
    _move_ctr += 0.5f;
    _hash.invert_move();
    
    // Обновляем счётчик 50 ходов
    bool is_irreversible = (move.attacker_type == PieceType::Pawn || move.defender_type != 255);
    if (is_irreversible) _fifty_moves_ctr = 0;
    else _fifty_moves_ctr += 0.5f;
    
    // Обновляем историю повторений
    if (is_irreversible) _history.clear();
    _history.push_back(_hash);
    
    // Проверяем, не остался ли король под шахом
    if (MoveGen::is_check(_pieces, side_to_move())) {
        // Откатываем изменения
        _pieces = old_pieces;
        _en_passant = old_en_passant;
        _w_l_castling = old_w_l;
        _w_s_castling = old_w_s;
        _b_l_castling = old_b_l;
        _b_s_castling = old_b_s;
        _fifty_moves_ctr = old_fifty;
        _hash = ZobristHash(_pieces, side_to_move() == PieceColor::Black,
                            _w_l_castling, _w_s_castling,
                            _b_l_castling, _b_s_castling);
        return false;
    }
    
    return true;
}

bool Position::is_checkmate() const {
    if (!MoveGen::is_check(_pieces, side_to_move())) return false;
    return !MoveGen::has_legal_moves(_pieces, side_to_move(), _en_passant,
                                      _w_l_castling, _w_s_castling,
                                      _b_l_castling, _b_s_castling);
}

bool Position::is_stalemate() const {
    if (MoveGen::is_check(_pieces, side_to_move())) return false;
    return !MoveGen::has_legal_moves(_pieces, side_to_move(), _en_passant,
                                      _w_l_castling, _w_s_castling,
                                      _b_l_castling, _b_s_castling);
}

bool Position::is_threefold_repetition() const {
    if (_history.empty()) return false;
    
    int count = 0;
    ZobristHash current = _history.back();
    
    for (const auto& h : _history) {
        if (h == current) count++;
        if (count >= 3) return true;
    }
    return false;
}

bool Position::is_fifty_move_rule() const {
    return _fifty_moves_ctr >= 100.0f;
}

MoveList Position::generate_moves() const {
    return MoveGen::generate_all_moves(_pieces, side_to_move(), _en_passant,
                                        _w_l_castling, _w_s_castling,
                                        _b_l_castling, _b_s_castling);
}

std::ostream& operator<<(std::ostream& os, const Position& pos) {
    os << pos._pieces;
    return os;
}