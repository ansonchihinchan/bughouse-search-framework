#include "eval/classical.h"
#include "eval/classical/activity.h"
#include "eval/classical/king_safety.h"
#include "eval/classical/material.h"
#include "eval/classical/mobility.h"
#include "eval/classical/pawn.h"
#include "eval/classical/piece_square.h"
#include "eval/classical/space.h"
#include "eval/classical/tempo.h"
#include "game/attacks.h"
#include "game/movegen.h"

ClassicalEvaluator::ClassicalEvaluator() {
  features_.push_back(std::make_unique<MaterialEvaluator>());
  features_.push_back(std::make_unique<PieceSquareEvaluator>());
  features_.push_back(std::make_unique<MobilityEvaluator>());
  features_.push_back(std::make_unique<PawnEvaluator>());
  features_.push_back(std::make_unique<KingSafetyEvaluator>());
  features_.push_back(std::make_unique<SpaceEvaluator>());
  features_.push_back(std::make_unique<TempoEvaluator>());
  features_.push_back(std::make_unique<ActivityEvaluator>());
}

int ClassicalEvaluator::evaluate(const Board &board, Colour side) const {
  ClassicalContext eval_context = make_classical_context(board);
  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  int white_score = score.final(eval_context.phase);
  return side == WHITE ? white_score : -white_score;
}

// in check, captures
bool ClassicalEvaluator::is_noisy(const Board &board) const {
  if (board.is_in_check())
    return true;

  auto moves = generate_pseudo_legal_moves(board);
  for (const Move &m : moves) {
    if (board.is_capture(m))
      return true;
  }
  return false;
}