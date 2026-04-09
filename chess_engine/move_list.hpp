#pragma once
#include <array>
#include "move.hpp"

// Максимальное количество ходов в одной позиции (известно из теории)
constexpr uint8_t MAX_MOVES = 218;

// Контейнер для хранения списка ходов
class MoveList {
public:
    MoveList() : _size(0) {}

    Move& operator[](uint8_t index) { return _moves[index]; }
    const Move& operator[](uint8_t index) const { return _moves[index]; }
    
    // Добавление хода в конец списка
    void push_back(const Move& move) {
        if (_size < MAX_MOVES) {
            _moves[_size++] = move;
        }
    }
    
    void clear() { _size = 0; }
    uint8_t size() const { return _size; }
    bool empty() const { return _size == 0; }
    
private:
    std::array<Move, MAX_MOVES> _moves{};
    uint8_t _size{0};
};