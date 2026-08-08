#include "eval/bughouse/prediction.h"
#include "eval/const.h"
#include "eval/score.h"
#include "game/piece_value.h"

#include <algorithm>

namespace {
float confidence_dampening(float volatility) {
  return 1.f /
         (1.f + PREDICTION_VOLATILITY_DAMPENING * std::max(0.f, volatility));
}

EvalScore anticipated_piece_score(const PredictionSummary &prediction) {
  struct Entry {
    PieceType pt;
    float probability;
  };
  const Entry entries[] = {
      {KNIGHT, prediction.probability_receive_knight},
      {BISHOP, prediction.probability_receive_bishop},
      {ROOK, prediction.probability_receive_rook},
      {QUEEN, prediction.probability_receive_queen},
  };

  int mid = 0, end = 0;
  for (const Entry &entry : entries) {
    if (entry.probability <= 0.f)
      continue;
    int value = PieceValue::PIECE_VALUE[entry.pt];
    mid += static_cast<int>(value * entry.probability *
                            PREDICTION_INCOMING_PIECE_WEIGHT_MID / 100.f);
    end += static_cast<int>(value * entry.probability *
                            PREDICTION_INCOMING_PIECE_WEIGHT_END / 100.f);
  }
  return EvalScore(mid, end);
}

} // namespace

EvalScore PredictionEvaluator::evaluate(const EvalContext &context) const {
  const PredictionSummary &prediction = context.communication.prediction;

  float damp = confidence_dampening(prediction.volatility);
  EvalScore score(0);

  // Predicted material flow
  float net_material =
      prediction.expected_incoming - prediction.expected_outgoing;
  score += EvalScore(static_cast<int>(net_material *
                                      PREDICTION_MATERIAL_WEIGHT_MID / 100.f),
                     static_cast<int>(net_material *
                                      PREDICTION_MATERIAL_WEIGHT_END / 100.f))
               .scale(damp);

  // Anticipated reinforcement
  score += anticipated_piece_score(prediction).scale(damp);

  // Attack / defence forecast
  if (prediction.attack_confidence > 0.f)
    score += EvalScore(PREDICTION_ATTACK_CONFIDENCE_BONUS_MID,
                       PREDICTION_ATTACK_CONFIDENCE_BONUS_END)
                 .scale(prediction.attack_confidence * damp);

  if (prediction.defence_confidence < 1.f)
    score -= EvalScore(PREDICTION_DEFENCE_RISK_PENALTY_MID,
                       PREDICTION_DEFENCE_RISK_PENALTY_END)
                 .scale((1.f - prediction.defence_confidence) * damp);

  return score;
}