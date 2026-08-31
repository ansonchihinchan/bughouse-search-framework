#include "eval/bughouse.h"
#include "eval/bughouse/drop.h"
#include "eval/bughouse/exchange.h"
#include "eval/bughouse/initiative.h"
#include "eval/bughouse/king_danger.h"
#include "eval/bughouse/partner.h"
#include "eval/bughouse/pocket.h"
#include "eval/bughouse/prediction.h"
#include "eval/const.h"
#include "game/attacks.h"
#include "game/movegen.h"
#include <algorithm>
#include <bit>

namespace {
bool has_check_drop(const Board &board, const Pocket &pocket, Colour mover) {
  init_attack_tables();

  Colour enemy = flip(mover);
  Bitboard enemy_king = board.bitboard_piece(make_piece(enemy, KING));
  if (!enemy_king)
    return false;

  Square ksq = static_cast<Square>(std::countr_zero(enemy_king));
  Bitboard occ = board.bitboard_all();
  Bitboard empty = ~occ;

  if (pocket.contains(KNIGHT) && (knight_attacks(ksq) & empty))
    return true;

  if ((pocket.contains(BISHOP) || pocket.contains(QUEEN)) &&
      (bishop_attacks(ksq, occ) & empty))
    return true;

  if ((pocket.contains(ROOK) || pocket.contains(QUEEN)) &&
      (rook_attacks(ksq, occ) & empty))
    return true;

  if (pocket.contains(PAWN)) {
    int back_rank_offset = (mover == WHITE) ? -8 : 8;
    for (int file_offset : {-1, 1}) {
      Square candidate = ksq + back_rank_offset + file_offset;
      if (candidate < 0 || candidate >= SQUARE_NO)
        continue;
      if (std::abs(file_of(candidate) - file_of(ksq)) != 1)
        continue;
      if (rank_of(candidate) == 0 || rank_of(candidate) == 7)
        continue;
      if (empty & (1ULL << candidate))
        return true;
    }
  }

  return false;
}

float pocket_weight_sum(const Pocket &pocket) {
  float total = 0.f;
  for (int pt = PAWN; pt <= QUEEN; pt++)
    total +=
        VOLATILITY_POCKET_WEIGHT[pt] * pocket.count(static_cast<PieceType>(pt));
  return total;
}

float king_exposure(const Board &board, Colour side) {
  Bitboard king_bb = board.bitboard_piece(make_piece(side, KING));
  if (!king_bb)
    return 0.f;

  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard zone = king_attacks(ksq) | king_bb;
  int zone_size = std::popcount(zone);
  if (zone_size == 0)
    return 0.f;

  int shielded =
      std::popcount(board.bitboard_piece(make_piece(side, PAWN)) & zone);
  return 1.f - static_cast<float>(shielded) / zone_size;
}

float board_exposure(const Board &board) {
  return std::max(king_exposure(board, WHITE), king_exposure(board, BLACK));
}

} // namespace

BughouseEvaluationConfig BughouseEvaluationConfig::independent() {
  return {false, false, false};
}

BughouseEvaluationConfig BughouseEvaluationConfig::request() {
  return {false, false, true};
}

BughouseEvaluationConfig BughouseEvaluationConfig::shared_value() {
  return {true, true, false};
}

BughouseEvaluator::BughouseEvaluator(BughouseEvaluationConfig config)
    : config_(config) {
  features_.push_back(std::make_unique<DropEvaluator>());
  if (config_.include_communication)
    communication_features_.push_back(std::make_unique<ExchangeEvaluator>());
  else
    features_.push_back(std::make_unique<ExchangeEvaluator>());
  features_.push_back(std::make_unique<InitiativeEvaluator>());
  features_.push_back(std::make_unique<KingDangerEvaluator>());
  if (config_.include_communication)
    communication_features_.push_back(std::make_unique<PartnerEvaluator>());
  features_.push_back(
      std::make_unique<PocketEvaluator>(config_.include_partner_pockets));
  if (config_.include_communication)
    communication_features_.push_back(std::make_unique<PredictionEvaluator>());
}

int BughouseEvaluator::evaluate(
    const BughousePosition &position, PlayerId root_player,
    const std::array<int64_t, PLAYER_NO> &remaining,
    const CommunicationContext &comm_context) const {
  EvalContext eval_context =
      make_eval_context(position, root_player, remaining, comm_context);
  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  int own_board = board_of(root_player);
  score += classical_.evaluate(eval_context.classical,
                               team_colour(root_player, own_board));

  if (config_.include_partner_board) {
    int partner_board = 1 - own_board;
    const Board &partner_board_ref = position.boards[partner_board];
    Colour partner_colour = team_colour(root_player, partner_board);
    int idx = static_cast<int>(partner_colour);

    int partner_score;
    if (cached_partner_valid_[idx] &&
        cached_partner_hash_[idx] == partner_board_ref.hash) {
      partner_score = cached_partner_score_[idx];
    } else {
      partner_score = classical_.evaluate(partner_board_ref, partner_colour);
      cached_partner_hash_[idx] = partner_board_ref.hash;
      cached_partner_score_[idx] = partner_score;
      cached_partner_valid_[idx] = true;
    }
    score += partner_score;
  }
  
  int final_score = score.final(eval_context.classical.phase);
  if (!communication_features_.empty()) {
    PlayerId communication_player =
        comm_context.origin_player == NO_PLAYER ? root_player
                                                : comm_context.origin_player;
    EvalContext communication_context = make_eval_context(
        position, communication_player, remaining, comm_context);
    EvalScore communication_score(0);
    for (const auto &feature : communication_features_)
      communication_score += feature->evaluate(communication_context);
    final_score +=
        team_sign(root_player, communication_player) *
        communication_score.final(communication_context.classical.phase);
  }
  return final_score;
}

bool BughouseEvaluator::is_noisy(const BughousePosition &position,
                                 PlayerId root_player) const {
  const Board &board = position.boards[board_of(root_player)];

  return classical_.is_noisy(board) ||
         has_check_drop(board, position.pockets[to_int(root_player)],
                        colour_of(root_player));
}

float BughouseEvaluator::volatility(const BughousePosition &position,
                                    PlayerId root_player) const {
  init_attack_tables();

  int own_board = board_of(root_player);
  float pocket =
      VOLATILITY_OWN_BOARD_WEIGHT *
          (pocket_weight_sum(position.pockets[to_int(root_player)]) +
           pocket_weight_sum(
               position.pockets[to_int(next_player(root_player))]));
  if (config_.include_partner_board) {
    pocket += VOLATILITY_PARTNER_BOARD_WEIGHT *
              (pocket_weight_sum(
                   position.pockets[to_int(partner_of(root_player))]) +
               pocket_weight_sum(position.pockets[to_int(
                   partner_of(next_player(root_player)))]));
  }
  pocket = std::clamp(pocket, 0.f, 1.f);

  float exposure =
      VOLATILITY_OWN_BOARD_WEIGHT * board_exposure(position.boards[own_board]);
  if (config_.include_partner_board) {
    int partner_board = 1 - own_board;
    exposure = std::max(
        exposure, VOLATILITY_PARTNER_BOARD_WEIGHT *
                      board_exposure(position.boards[partner_board]));
  }
  exposure = std::clamp(exposure, 0.f, 1.f);

  return std::clamp(pocket + VOLATILITY_EXPOSURE_SCALE * exposure * pocket, 0.f,
                    1.f);
}