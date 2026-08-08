#include "eval/bughouse/partner.h"
#include "eval/const.h"
#include "eval/score.h"

namespace {
float eta_weight(int eta_plies) {
  if (eta_plies < 0 || eta_plies >= PARTNER_ETA_HORIZON_PLIES)
    return 0.f;
  return static_cast<float>(PARTNER_ETA_HORIZON_PLIES - eta_plies) /
         static_cast<float>(PARTNER_ETA_HORIZON_PLIES);
}

float urgency_weight(Urgency urgency) {
  return PARTNER_URGENCY_WEIGHT[static_cast<int>(urgency)];
}

} // namespace

EvalScore PartnerEvaluator::evaluate(const EvalContext &context) const {
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

              ? EvalScore(PARTNER_REQUEST_FULFILLED_BONUS_MID,
                          PARTNER_REQUEST_FULFILLED_BONUS_END)
              : EvalScore(-PARTNER_REQUEST_UNMET_PENALTY_MID,
                          -PARTNER_REQUEST_UNMET_PENALTY_END);

      score += base.scale(weight);
    }
  }

  // Strategy request
  if (partner.strat_request.strat == StrategyType::AttackNow) {
    float weight = partner.strat_request.confidence *
                   urgency_weight(partner.strat_request.urgency);
    score += EvalScore(PARTNER_ATTACK_READINESS_BONUS_MID,
                       PARTNER_ATTACK_READINESS_BONUS_END)
                 .scale(partner.strat_request.confidence);
  } else if (partner.strat_request.strat == StrategyType::Defend) {
    float weight = partner.strat_request.confidence *
                   urgency_weight(partner.strat_request.urgency);
    score += EvalScore(PARTNER_DEFENCE_READINESS_BONUS_MID,
                       PARTNER_DEFENCE_READINESS_BONUS_END)
                 .scale(weight);
  }

  if (partner.strat_request.strat == StrategyType::TradeEverything &&
      partner.material_balance > 0) {
    score += EvalScore(PARTNER_STRATEGY_ALIGNMENT_BONUS_MID,
                       PARTNER_STRATEGY_ALIGNMENT_BONUS_END)
                 .scale(partner.strat_request.confidence);
  } else if (partner.strat_request.strat == StrategyType::AvoidTrades &&
             partner.material_balance < 0) {
    score += EvalScore(PARTNER_STRATEGY_ALIGNMENT_BONUS_MID,
                       PARTNER_STRATEGY_ALIGNMENT_BONUS_END)
                 .scale(partner.strat_request.confidence);
  }

  return score;
}