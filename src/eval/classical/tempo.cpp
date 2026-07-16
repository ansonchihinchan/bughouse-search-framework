#include "eval/classical/tempo.h"

namespace {
constexpr int TEMPO_BONUS = 12;
}

EvalScore TempoEvaluator::evaluate(const EvalContext &context) const {
  int score = 0;
  for (int b = 0; b < BOARD_NO; b++) {
    Colour side_to_move = context.position.boards[b].sideToMove;
    Colour ours = team_colour(context.search.root_player, b);
    score += (side_to_move == ours) ? TEMPO_BONUS : -TEMPO_BONUS;
  }
  return EvalScore(score);
}