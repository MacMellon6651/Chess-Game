#pragma once
#include "pieces.hpp"
#include "zobrist.hpp"
#include "move_list.hpp"
#include <vector>
#include <cmath>

// Класс полной позиции на доске
class Position {
public:
    Position();
    explicit Position(const std::string& fen);  // Полная FEN-строка
    
    // Применение хода к позиции
    bool apply_move(const Move& move);
    
    // Генерация всех легальных ходов из текущей позиции
    MoveList generate_moves() const;
    
    // Проверка окончаний партии
    bool is_checkmate() const;
    bool is_stalemate() const;
    bool is_threefold_repetition() const;
    bool is_fifty_move_rule() const;
    
    // Геттеры
    const Pieces& pieces() const { return _pieces; }
    
    uint8_t side_to_move() const { 
        return (static_cast<int>(_move_ctr) % 2 == 0) ? PieceColor::White : PieceColor::Black;
    }
    
    uint8_t en_passant() const { return _en_passant; }
    bool w_l_castling() const { return _w_l_castling; }
    bool w_s_castling() const { return _w_s_castling; }
    bool b_l_castling() const { return _b_l_castling; }
    bool b_s_castling() const { return _b_s_castling; }
    
    // Для FEN
    std::string get_fen() const;
    float fifty_moves() const { return _fifty_moves_ctr; }
    float move_number() const { return _move_ctr; }
    
    // Вывод в консоль
    friend std::ostream& operator<<(std::ostream& os, const Position& pos);
    
private:
    // Вспомогательные методы для apply_move
    void _add_piece(uint8_t square, uint8_t type, uint8_t side);
    void _remove_piece(uint8_t square, uint8_t type, uint8_t side);
    void _update_castling_rights(uint8_t from);
    
    Pieces _pieces;
    uint8_t _en_passant{255};
    
    bool _w_l_castling{true};
    bool _w_s_castling{true};
    bool _b_l_castling{true};
    bool _b_s_castling{true};
    
    float _move_ctr{0.0f};           // 0.0 = ход белых, 0.5 = ход чёрных
    ZobristHash _hash;
    float _fifty_moves_ctr{0.0f};
    std::vector<ZobristHash> _history;
};