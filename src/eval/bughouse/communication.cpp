#include "eval/bughouse/communication.h"
#include "eval/const.h"

namespace {
EvalScore scale(EvalScore score, float weight) {
  return EvalScore(static_cast<int>(score.mid_game() * weight),
                   static_cast<int>(score.end_game() * weight));
}

float eta_weight(int eta_plies) {
  if (eta_plies < 0 || eta_plies >= COMM_ETA_HORIZON_PLIES)
    return 0.f;
  return static_cast<float>(COMM_ETA_HORIZON_PLIES - eta_plies) /
         static_cast<float>(COMM_ETA_HORIZON_PLIES);
}

float urgency_weight(Urgency urgency) {
  return COMM_URGENCY_WEIGHT[static_cast<int>(urgency)];
}

} // namespace

EvalScore CommunicationEvaluator::evaluate(const EvalContext &context) const {
  const PartnerContext &partner = context.communication.partner;
  const PredictionSummary &prediction = context.communication.prediction;
  const BughouseContext &bughouse = context.bughouse;

  EvalScore score(0);

  // Piece request
  if (partner.piece_request.piece != NO_PIECE_TYPE) {
    float weight = partner.piece_request.confidence *
                   eta_weight(partner.piece_request.eta_plies) *
                   urgency_weight(partner.piece_request.urgency);

    if (weight > 0.f) {
      EvalScore base =
          bughouse.own_pocket().contains(partner.piece_request.piece)

              ? EvalScore(COMM_REQUEST_FULFILLED_BONUS_MID,
                          COMM_REQUEST_FULFILLED_BONUS_END)
              : EvalScore(-COMM_REQUEST_UNMET_PENALTY_MID,
                          -COMM_REQUEST_UNMET_PENALTY_END);

      score += scale(base, weight);
    }
  }

  // Predicted material flow
  float net_prediction =
      prediction.expected_incoming - prediction.expected_outgoing;
  score +=
      EvalScore(static_cast<int>(net_prediction *
                                 COMM_PREDICTED_MATERIAL_WEIGHT_MID / 100.f),
                static_cast<int>(net_prediction *
                                 COMM_PREDICTED_MATERIAL_WEIGHT_END / 100.f));

  // Strategy request
  if (partner.strat_request.strat == StrategyType::AttackNow) {
    float weight = partner.strat_request.confidence *
                   urgency_weight(partner.strat_request.urgency) *
                   prediction.attack_confidence;
    score += scale(EvalScore(COMM_ATTACK_READINESS_BONUS_MID,
                             COMM_ATTACK_READINESS_BONUS_END),
                   weight);
  } else if (partner.strat_request.strat == StrategyType::Defend) {
    float weight = partner.strat_request.confidence *
                   urgency_weight(partner.strat_request.urgency) *
                   prediction.defence_confidence;
    score += scale(EvalScore(COMM_DEFENCE_READINESS_BONUS_MID,
                             COMM_DEFENCE_READINESS_BONUS_END),
                   weight);
  }

  if (partner.strat_request.strat == StrategyType::TradeEverything &&
      partner.material_balance > 0) {
    score += scale(EvalScore(COMM_STRATEGY_ALIGNMENT_BONUS_MID,
                             COMM_STRATEGY_ALIGNMENT_BONUS_END),
                   partner.strat_request.confidence);
  } else if (partner.strat_request.strat == StrategyType::AvoidTrades &&
             partner.material_balance < 0) {
    score += scale(EvalScore(COMM_STRATEGY_ALIGNMENT_BONUS_MID,
                             COMM_STRATEGY_ALIGNMENT_BONUS_END),
                   partner.strat_request.confidence);
  }

  return score;
}