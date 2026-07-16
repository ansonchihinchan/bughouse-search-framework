#include "eval/classical.h"
#include "eval/classical/activity.h"
#include "eval/classical/king_safety.h"
#include "eval/classical/material.h"
#include "eval/classical/mobility.h"
#include "eval/classical/pawn.h"
#include "eval/classical/piece_square.h"
#include "eval/classical/space.h"
#include "eval/classical/tempo.h"

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

int ClassicalEvaluator::evaluate(const BughousePosition &position,
                                 const SearchContext &search_context) const {
  EvalContext eval_context = to_context(position, search_context);

  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  return score.final(eval_context.material_info.phase);
}