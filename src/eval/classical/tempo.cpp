#include "eval/classical/tempo.h"

namespace {
constexpr int TEMPO_BONUS = 12;
}

EvalScore TempoEvaluator::evaluate(const EvalContext &context) const {
  return EvalScore(context.board.sideToMove == WHITE ? TEMPO_BONUS
                                                     : -TEMPO_BONUS);
}