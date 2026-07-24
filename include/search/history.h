#pragma once

#include "game/board.h"
#include "game/types.h"
#include <algorithm>
#include <array>

class History {
public:
  static constexpr int MAX_VALUE = 1 << 20;

  void add(Piece piece, Square to, int depth) {
    int &slot = history_[piece.index()][to];
    slot = std::clamp(slot + depth * depth, -MAX_VALUE, MAX_VALUE);
  }

  int score(Piece piece, Square to) const {
    return history_[piece.index()][to];
  }

  void age() {
    for (auto &row : history_)
      for (auto &value : row)
        value /= 2;
  }

  void clear() {
    for (auto &row : history_)
      row.fill(0);
  }

private:
  std::array<std::array<int, SQUARE_NO>, PIECE_NO> history_{};
};