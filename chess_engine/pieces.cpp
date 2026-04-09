#include "pieces.hpp"
#include <sstream>

Pieces::Pieces(const std::string& fen) {
    // Очищаем все битборды
    for (auto& color : piece_bitboards)
        for (auto& bb : color)
            bb = 0;
    
    uint8_t x = 0;
    uint8_t y = 7;  // Начинаем с последней строки (8-я)
    uint8_t side;
    
    for (char ch : fen) {
        if (ch == '/') {
            // Переход на следующую строку
            x = 0;
            y--;
        }
        else if (std::isdigit(ch)) {
            // Пропуск пустых клеток
            x += (ch - '0');
        }
        else {
            // Определяем цвет фигуры (заглавная - белые, строчная - чёрные)
            if (std::isupper(ch)) {
                side = PieceColor::White;
                ch = std::tolower(ch);
            }
            else {
                side = PieceColor::Black;
            }
            
            uint8_t square = y * 8 + x;
            
            // Устанавливаем бит для соответствующей фигуры
            switch (ch) {
                case 'p': BitboardOps::set_1(piece_bitboards[side][PieceType::Pawn], square); break;
                case 'n': BitboardOps::set_1(piece_bitboards[side][PieceType::Knight], square); break;
                case 'b': BitboardOps::set_1(piece_bitboards[side][PieceType::Bishop], square); break;
                case 'r': BitboardOps::set_1(piece_bitboards[side][PieceType::Rook], square); break;
                case 'q': BitboardOps::set_1(piece_bitboards[side][PieceType::Queen], square); break;
                case 'k': BitboardOps::set_1(piece_bitboards[side][PieceType::King], square); break;
                default: break;
            }
            x++;
        }
    }
    
    update_bitboards();
}

void Pieces::update_bitboards() {
    // Собираем все белые фигуры
    side_bitboards[PieceColor::White] = 
        piece_bitboards[PieceColor::White][PieceType::Pawn] |
        piece_bitboards[PieceColor::White][PieceType::Knight] |
        piece_bitboards[PieceColor::White][PieceType::Bishop] |
        piece_bitboards[PieceColor::White][PieceType::Rook] |
        piece_bitboards[PieceColor::White][PieceType::Queen] |
        piece_bitboards[PieceColor::White][PieceType::King];
    
    // Собираем все чёрные фигуры
    side_bitboards[PieceColor::Black] = 
        piece_bitboards[PieceColor::Black][PieceType::Pawn] |
        piece_bitboards[PieceColor::Black][PieceType::Knight] |
        piece_bitboards[PieceColor::Black][PieceType::Bishop] |
        piece_bitboards[PieceColor::Black][PieceType::Rook] |
        piece_bitboards[PieceColor::Black][PieceType::Queen] |
        piece_bitboards[PieceColor::Black][PieceType::King];
    
    // Инверсии (все клетки, не занятые фигурами соответствующего цвета)
    inversion_side_bitboards[PieceColor::White] = ~side_bitboards[PieceColor::White];
    inversion_side_bitboards[PieceColor::Black] = ~side_bitboards[PieceColor::Black];
    
    // Все фигуры на доске
    all = side_bitboards[PieceColor::White] | side_bitboards[PieceColor::Black];
    empty = ~all;
}

bool Pieces::operator==(const Pieces& other) const {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 6; ++j) {
            if (piece_bitboards[i][j] != other.piece_bitboards[i][j])
                return false;
        }
    }
    return true;
}

uint8_t Pieces::piece_at(uint8_t square) const {
    for (int side = 0; side < 2; ++side) {
        for (int type = 0; type < 6; ++type) {
            if (BitboardOps::get_bit(piece_bitboards[side][type], square))
                return type;
        }
    }
    return 255;  // Пусто
}

uint8_t Pieces::color_at(uint8_t square) const {
    if (BitboardOps::get_bit(side_bitboards[PieceColor::White], square))
        return PieceColor::White;
    if (BitboardOps::get_bit(side_bitboards[PieceColor::Black], square))
        return PieceColor::Black;
    return 255;  // Пусто
}

std::ostream& operator<<(std::ostream& os, const Pieces& pieces) {
    for (int y = 7; y >= 0; --y) {
        os << (y + 1) << " ";
        for (int x = 0; x < 8; ++x) {
            uint8_t square = y * 8 + x;
            os << "|";
            
            // Белые фигуры
            if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Pawn], square)) os << "♙";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Knight], square)) os << "♘";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Bishop], square)) os << "♗";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Rook], square)) os << "♖";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::Queen], square)) os << "♕";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::White][PieceType::King], square)) os << "♔";
            // Чёрные фигуры
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Pawn], square)) os << "♟";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Knight], square)) os << "♞";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Bishop], square)) os << "♝";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Rook], square)) os << "♜";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::Queen], square)) os << "♛";
            else if (BitboardOps::get_bit(pieces.piece_bitboards[PieceColor::Black][PieceType::King], square)) os << "♚";
            // Пустая клетка
            else os << " ";
        }
        os << "|\n";
    }
    os << "   a b c d e f g h\n";
    return os;
}