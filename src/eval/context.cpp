#include "eval/context.h"
#include "eval/classical/king_safety.h"
#include "eval/classical/material.h"
#include "eval/const.h"
#include "eval/types.h"
#include "game/attacks.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace {
constexpr float DANGER_THRESHOLD = 0.f;
constexpr float STALL_THRESHOLD = 0.35f;
constexpr float STALL_MATERIAL_UNIT = 550.f;

uint64_t hash_combine(uint64_t seed, uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

uint64_t hash_float(float value) {
  return static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
}
} // namespace

bool is_dangerous(const Board &board, int phase) {
  KingSafetyEvaluator eval;
  return eval.evaluate(make_classical_context(board)).final(phase) <=
         DANGER_THRESHOLD;
}

bool should_stall(float stall_intent) {
  return stall_intent >= STALL_THRESHOLD;
}

PartnerContext make_partner_context(const BughousePosition &position,
                                    PlayerId partner) {
  const Board &board = position.boards[board_of(partner)];

  PartnerContext partner_context;
  ClassicalContext classical_context = make_classical_context(board);
  MaterialEvaluator material;
  int phase = classical_context.phase;

  partner_context.material_balance =
      material.evaluate(classical_context).final(phase);

  KingSafetyEvaluator king_safety;
  partner_context.king_danger =
      -king_safety.evaluate(classical_context).final(phase);

  partner_context.phase = phase;

  // TODO
  partner_context.stall_intent = 0.f;

  return partner_context;
}

PredictionSummary make_prediction_summary(const BughousePosition &position,
                                          PlayerId root_player,
                                          Message message) {
  PredictionSummary summary;

  PlayerId partner = partner_of(root_player);
  PartnerContext partner_context = make_partner_context(position, partner);

  // TODO: receive_probability population
  const PieceRequest &request = message.piece_request;
  if (request.piece != NO_PIECE_TYPE) {
    float weight =
        std::clamp(request.confidence * eta_weight(request.eta_plies) *
                       urgency_weight(request.urgency),
                   0.f, 1.f);
    summary.donate_probability[request.piece] = weight;
    summary.expected_outgoing_value +=
        weight * PieceValue::effective_value(request.piece);
  }

  const StrategyRequest &strat = message.strat_request;
  if (strat.strat == StrategyType::AttackNow) {
    summary.attack_confidence =
        std::clamp(strat.confidence * urgency_weight(strat.urgency), 0.f, 1.f);
    summary.expected_incoming_value +=
        summary.attack_confidence * PieceValue::PIECE_VALUE[KNIGHT];
  }

  float danger_ratio =
      std::clamp(static_cast<float>(partner_context.king_danger) /
                     PARTNER_KING_DANGER_CLAMP,
                 0.f, 1.f);
  summary.defence_confidence = 1.f - danger_ratio;

  summary.volatility =
      danger_ratio + (request.piece != NO_PIECE_TYPE
                          ? request.confidence * urgency_weight(request.urgency)
                          : 0.f);

  return summary;
}

CommunicationContext
make_communication_context(const BughousePosition &position,
                           PlayerId root_player, const Channel &channel) {
  CommunicationContext context;
  const PlayerId partner = partner_of(root_player);
  context.message = channel.latest(partner);
  context.partner = make_partner_context(position, partner);

  PlayerId other_side_partner = partner_of(next_player(root_player));
  context.partner_by_colour[colour_of(partner)] = context.partner;
  context.partner_by_colour[colour_of(other_side_partner)] =
      make_partner_context(position, other_side_partner);

  context.prediction =
      make_prediction_summary(position, root_player, context.message);
  return context;
}

uint64_t communication_hash(const CommunicationContext &context) {
  uint64_t hash = 0xcbf29ce484222325ULL;

  const PartnerContext &partner = context.partner;
  hash = hash_combine(hash, static_cast<uint64_t>(partner.material_balance));
  hash = hash_combine(hash, static_cast<uint64_t>(partner.king_danger));
  hash = hash_combine(hash, static_cast<uint64_t>(partner.phase));
  hash = hash_combine(hash, hash_float(partner.stall_intent));

  const Message &message = context.message;
  hash = hash_combine(hash, static_cast<uint64_t>(to_int(message.sender)));
  hash = hash_combine(hash, static_cast<uint64_t>(message.move_no));
  hash = hash_combine(hash, static_cast<uint64_t>(message.piece_request.piece));
  hash = hash_combine(hash, hash_float(message.piece_request.confidence));
  hash =
      hash_combine(hash, static_cast<uint64_t>(message.piece_request.urgency));
  hash = hash_combine(hash,
                      static_cast<uint64_t>(message.piece_request.eta_plies));
  hash = hash_combine(hash, static_cast<uint64_t>(message.strat_request.strat));
  hash = hash_combine(hash, hash_float(message.strat_request.confidence));
  hash =
      hash_combine(hash, static_cast<uint64_t>(message.strat_request.urgency));

  const PredictionSummary &prediction = context.prediction;
  for (int pt = 0; pt < PIECE_TYPE_NO; pt++) {
    hash = hash_combine(hash, hash_float(prediction.receive_probability[pt]));
    hash = hash_combine(hash, hash_float(prediction.donate_probability[pt]));
  }
  hash = hash_combine(hash, hash_float(prediction.expected_incoming_value));
  hash = hash_combine(hash, hash_float(prediction.expected_outgoing_value));
  hash = hash_combine(hash, hash_float(prediction.attack_confidence));
  hash = hash_combine(hash, hash_float(prediction.defence_confidence));
  hash = hash_combine(hash, hash_float(prediction.volatility));

  return hash;
}