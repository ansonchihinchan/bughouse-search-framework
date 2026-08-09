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
  int mid = 0, end = 0;
  for (int i = 0; i < PIECE_TYPE_NO; i++) {
    float probability = prediction.receive_probability[i];
    if (probability <= 0.f)
      continue;
    int value = PieceValue::PIECE_VALUE[i];
    mid += static_cast<int>(value * probability *
                            PREDICTION_INCOMING_PIECE_WEIGHT_MID / 100.f);
    end += static_cast<int>(value * probability *
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
      prediction.expected_incoming_value - prediction.expected_outgoing_value;
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