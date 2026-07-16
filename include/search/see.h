#pragma once

#include "game/bughouse.h"
#include "search/types.h"
#include <array>

namespace SEE {
inline constexpr std::array<int, PIECE_TYPE_NO> PIECE_VALUE{0,   100, 320,  330,
                                                            550, 900, 20000};

inline constexpr std::array<int, PIECE_TYPE_NO> POCKET_BONUS{0,  40,  60, 60,
                                                             90, 150, 0};

struct Result {
  int score = 0;
  bool undefended = false;
  bool king_exposed = false;
};

Result see_result(const Board &board, Move move);

inline int see_score(const Board &board, Move move) {
  return see_result(board, move).score;
}
}; // namespace SEE