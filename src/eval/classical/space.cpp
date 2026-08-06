#include "eval/classical/space.h"
#include "eval/const.h"
#include <bit>

namespace {
int space_score(const Board &board, const AttackInfo &attack_info,
                Colour colour) {
  // Space only matters with enough pieces left to fight over it
  int minors_majors =
      std::popcount(board.bitboard_colour(colour)) -
      std::popcount(board.bitboard_piece(make_piece(colour, PAWN))) - 1;
  if (minors_majors < 2)
    return 0;

  Bitboard own_half = (colour == WHITE) ? WHITE_SPACE_RANKS : BLACK_SPACE_RANKS;
  Bitboard controlled = attack_info.attacks[colour] & own_half;
  return SPACE_WEIGHT * std::popcount(controlled);
}
} // namespace

EvalScore SpaceEvaluator::evaluate(const ClassicalContext &context) const {
  const Board &board = context.board;

  return EvalScore(space_score(board, context.attack_info, WHITE) -
                   space_score(board, context.attack_info, BLACK));
}