#pragma once

#include "game/bughouse.h"
#include "game/piece_value.h"
#include "search/types.h"
#include <array>

namespace SEE {
inline constexpr auto &PIECE_VALUE = PieceValue::PIECE_VALUE;
inline constexpr auto &POCKET_BONUS = PieceValue::POCKET_BONUS;

struct Result {
  int score = 0;
  bool undefended = false;
  bool king_exposed = false;
};

Result see_result(const Board &board, Move move);

Result see_drop_result(const Board &board, PieceType pt, Square to);

inline int see_score(const Board &board, Move move) {
  return see_result(board, move).score;
}

inline int see_drop_score(const Board &board, PieceType pt, Square to) {
  return see_drop_result(board, pt, to).score;
}
}; // namespace SEE