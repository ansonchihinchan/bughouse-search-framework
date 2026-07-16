#include "eval/classical/space.h"
#include <bit>

namespace {
// White: ranks 2-3, Black: ranks 6-7
constexpr Bitboard WHITE_SPACE_RANKS = 0x0000000000FFFF00ULL;
constexpr Bitboard BLACK_SPACE_RANKS = 0x00FFFF0000000000ULL;
constexpr int SPACE_WEIGHT = 2;

int space_score(const Board &board, const AttackInfo &attack_info,
                int board_idx, Colour colour) {
  // Space only matters with enough pieces left to fight over it
  int minors_majors =
      std::popcount(board.bitboard_colour(colour)) -
      std::popcount(board.bitboard_piece(make_piece(colour, PAWN))) - 1;
  if (minors_majors < 2)
    return 0;

  Bitboard own_half = (colour == WHITE) ? WHITE_SPACE_RANKS : BLACK_SPACE_RANKS;
  Bitboard controlled = attack_info.attacks[board_idx][colour] & own_half;
  return SPACE_WEIGHT * std::popcount(controlled);
}
} // namespace

EvalScore SpaceEvaluator::evaluate(const EvalContext &context) const {
  int score = 0;
  for (int b = 0; b < BOARD_NO; b++) {
    const Board &board = context.position.boards[b];
    Colour ours = team_colour(context.search.root_player, b);
    score += space_score(board, context.attack_info, b, ours) -
             space_score(board, context.attack_info, b, flip(ours));
  }
  return EvalScore(score);
}