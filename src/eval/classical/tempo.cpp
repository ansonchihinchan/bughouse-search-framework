#include "eval/classical/tempo.h"
#include "eval/const.h"

EvalScore TempoEvaluator::evaluate(const ClassicalContext &context) const {
  return EvalScore(context.board.sideToMove == WHITE ? TEMPO_BONUS
                                                     : -TEMPO_BONUS);
}