#pragma once

#include "game/board.h"
#include "search/types.h"
#include <array>

class CounterMove {
public:
  void update(const DetailedMove &prev, Move move) {
    if (prev.move.is_none() || prev.piece.is_empty())
      return;
    counter_move_[prev.piece.index()][prev.move.to] = move;
  }

  Move counter_move(Piece piece, Square to) const {
    return counter_move_[piece.index()][to];
  }

private:
  std::array<std::array<Move, SQUARE_NO>, PIECE_NO> counter_move_{};
};