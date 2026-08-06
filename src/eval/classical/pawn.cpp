#include "eval/classical/pawn.h"
#include "eval/const.h"
#include <bit>

namespace {
int pawn_score(const PawnInfo &info, Colour colour) {
  int score = 0;
  score += PASSED_BONUS * std::popcount(info.passed[colour]);
  score -= ISOLATED_PENALTY * std::popcount(info.isolated[colour]);
  score -= DOUBLED_PENALTY * std::popcount(info.doubled[colour]);
  return score;
}
} // namespace

EvalScore PawnEvaluator::evaluate(const ClassicalContext &context) const {
  return EvalScore(pawn_score(context.pawn_info, WHITE) -
                   pawn_score(context.pawn_info, BLACK));
}