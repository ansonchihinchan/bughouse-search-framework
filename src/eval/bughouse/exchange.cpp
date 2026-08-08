#include "eval/bughouse/exchange.h"
#include "eval/const.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <utility>

namespace {
Bitboard hanging(const Board &board, Colour colour, Bitboard attacked_by,
                 Bitboard defended_by) {
  return board.bitboard_colour(colour) & attacked_by & ~defended_by;
}

Bitboard contested(const Board &board, Colour colour, Bitboard attacked_by,
                   Bitboard defended_by) {
  return board.bitboard_colour(colour) & attacked_by & defended_by;
}

float urgency_weight(Urgency urgency) {
  return COMM_URGENCY_WEIGHT[static_cast<int>(urgency)];
}

float eta_weight(int eta_plies) {
  if (eta_plies < 0 || eta_plies >= COMM_ETA_HORIZON_PLIES)
    return 0.f;
  return static_cast<float>(COMM_ETA_HORIZON_PLIES - eta_plies) /
         static_cast<float>(COMM_ETA_HORIZON_PLIES);
}

template <typename MultiplierFunc>
void accumulate(const Board &board, Bitboard squares, float fraction,
                double &mid_total, double &end_total,
                MultiplierFunc multiplier) {
  while (squares) {
    Square sq = static_cast<Square>(std::countr_zero(squares));
    squares &= squares - 1;

    PieceType pt = board.piece_on(sq).type;
    if (pt == KING || pt == NO_PIECE_TYPE)
      continue;

    int value = PieceValue::effective_value(pt);
    auto [mult_mid, mult_end] = multiplier(pt);

    mid_total += value * fraction * mult_mid;
    end_total += value * fraction * mult_end;
  }
}

} // namespace

EvalScore ExchangeEvaluator::evaluate(const EvalContext &context) const {
  const Board &board = context.classical.board;
  const AttackInfo &attack_info = context.classical.attack_info;
  const PartnerContext &partner = context.communication.partner;

  Colour us = colour_of_player(context.bughouse.root_player);
  Colour them = flip(us);

  Bitboard theirs_hanging =
      hanging(board, them, attack_info.attacks[us], attack_info.attacks[them]);
  Bitboard theirs_contested = contested(board, them, attack_info.attacks[us],
                                        attack_info.attacks[them]);
  Bitboard ours_hanging =
      hanging(board, us, attack_info.attacks[them], attack_info.attacks[us]);
  Bitboard ours_contested =
      contested(board, us, attack_info.attacks[them], attack_info.attacks[us]);

  int partner_danger_scale = std::clamp(static_cast<int>(partner.king_danger),
                                        0, PARTNER_KING_DANGER_CLAMP);
  float danger_ratio =
      static_cast<float>(partner_danger_scale) / PARTNER_KING_DANGER_CLAMP;

  float request_weight = partner.piece_request.confidence *
                         urgency_weight(partner.piece_request.urgency) *
                         eta_weight(partner.piece_request.eta_plies);

  float danger_signal_weight =
      partner.danger ? partner.strat_request.confidence *
                           urgency_weight(partner.strat_request.urgency)
                     : 0.f;

  auto help_multiplier = [&](PieceType pt) -> std::pair<float, float> {
    float requested_mid =
        (partner.piece_request.piece == pt) ? EXCHANGE_REQUEST_BONUS_MID : 0.f;
    float requested_end =
        (partner.piece_request.piece == pt) ? EXCHANGE_REQUEST_BONUS_END : 0.f;
    float help_bonus = partner.danger ? EXCHANGE_PARTNER_HELP_BONUS : 0.f;

    return {EXCHANGE_BASE_MULTIPLIER + requested_mid + help_bonus,
            EXCHANGE_BASE_MULTIPLIER + requested_end + help_bonus};
  };

  auto threat_multiplier = [&](PieceType) -> std::pair<float, float> {
    float flag_mid =
        partner.danger ? EXCHANGE_THREAT_DANGER_FLAG_BONUS_MID : 0.f;
    float flag_end =
        partner.danger ? EXCHANGE_THREAT_DANGER_FLAG_BONUS_END : 0.f;

    return {EXCHANGE_BASE_MULTIPLIER +
                EXCHANGE_THREAT_DANGER_WEIGHT_MID * danger_ratio + flag_mid,
            EXCHANGE_BASE_MULTIPLIER +
                EXCHANGE_THREAT_DANGER_WEIGHT_END * danger_ratio + flag_end};
  };

  double help_mid = 0.0, help_end = 0.0;
  accumulate(board, theirs_hanging, 1.0f, help_mid, help_end, help_multiplier);
  accumulate(board, theirs_contested, EXCHANGE_CONTESTED_FRACTION, help_mid,
             help_end, help_multiplier);

  double threat_mid = 0.0, threat_end = 0.0;
  accumulate(board, ours_hanging, 1.0f, threat_mid, threat_end,
             threat_multiplier);
  accumulate(board, ours_contested, EXCHANGE_CONTESTED_FRACTION, threat_mid,
             threat_end, threat_multiplier);

  return EvalScore(static_cast<int>(std::lround(help_mid - threat_mid)),
                   static_cast<int>(std::lround(help_end - threat_end)));
}