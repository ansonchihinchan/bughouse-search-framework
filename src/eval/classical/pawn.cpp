#include "eval/classical/pawn.h"
#include <bit>

namespace {
constexpr int PASSED_BONUS = 20;
constexpr int ISOLATED_PENALTY = 15;
constexpr int DOUBLED_PENALTY = 10;

int pawn_score(const PawnInfo &info, int board, Colour colour) {
  int score = 0;
  score += PASSED_BONUS * std::popcount(info.passed[board][colour]);
  score -= ISOLATED_PENALTY * std::popcount(info.isolated[board][colour]);
  score -= DOUBLED_PENALTY * std::popcount(info.doubled[board][colour]);
  return score;
}
} // namespace

EvalScore PawnEvaluator::evaluate(const EvalContext &context) const {
  int score = 0;
  for (int b = 0; b < BOARD_NO; b++) {
    Colour ours = team_colour(context.search.root_player, b);
    score += pawn_score(context.pawn_info, b, ours) -
             pawn_score(context.pawn_info, b, flip(ours));
  }
  return EvalScore(score);
}