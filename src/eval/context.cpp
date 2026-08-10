#include "eval/context.h"
#include "eval/classical/king_safety.h"
#include "eval/classical/material.h"
#include "eval/types.h"
#include "game/attacks.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>

namespace {
constexpr float DANGER_THRESHOLD = 0.f;
constexpr float STALL_THRESHOLD = 0.35f;
constexpr float STALL_MATERIAL_UNIT = 550.f;
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
  // TODO
}

CommunicationContext
make_communication_context(const BughousePosition &position,
                           PlayerId root_player, const Channel &channel) {
  CommunicationContext context;
  const PlayerId partner = partner_of(root_player);
  context.message = channel.latest(partner);
  context.partner = make_partner_context(position, partner);
  context.prediction =
      make_prediction_summary(position, root_player, context.message);
  return context;
}