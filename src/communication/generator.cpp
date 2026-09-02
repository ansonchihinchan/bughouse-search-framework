#include "communication/generator.h"
#include "game/attacks.h"
#include "game/piece_value.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace {
constexpr int STRATEGIC_SCORE_THRESHOLD = 350;
constexpr int FORCING_SCORE_THRESHOLD = 600;

constexpr int FLAG_TARGET_TIME_THRESHOLD_MS = 15000;
constexpr int FLAG_MIN_TIME_EDGE_MS = 5000;

constexpr int ETA_USABLE = 1;
constexpr int ETA_IMMEDIATE_CAPTURE = 2;
constexpr int ETA_ON_PARTNER_BOARD = 4;
constexpr int ETA_CURRENTLY_UNAVAILABLE = 6;

constexpr int PIECE_CHECKING_SQUARE_WEIGHT = 1000;
constexpr float FULL_GEOMETRY_SQUARES = 4.0f;
constexpr float POCKET_SCARCITY_FACTOR = 0.25f;

constexpr float CONFIDENCE_BASE = 0.55f;
constexpr float DROP_CHECK_CONFIDENCE_STEP = 0.07f;

constexpr float DEFEND_IN_CHECK_CONFIDENCE = 0.95f;
constexpr float MAX_DEFEND_CONFIDENCE = 0.90f;
constexpr float MAX_ATTACK_CONFIDENCE = 0.95f;
constexpr float MAX_SCORE_CONFIDENCE = 0.90f;
constexpr float SCORE_CONFIDENCE_SCALE = 2000.0f;

constexpr float FLAG_CONFIDENCE_TIME_WEIGHT = 0.25f;
constexpr float FLAG_CONFIDENCE_EDGE_WEIGHT = 0.20f;
constexpr float FLAG_EDGE_CONFIDENCE_SCALE_MS = 20000.0f;

constexpr float STRATEGIC_CONFIDENCE_BASE = 0.50f;
constexpr float STALL_CONFIDENCE_BASE = 0.60f;
constexpr float STALL_SCORE_SCALE = 1400.0f;
constexpr float MIN_STALL_CONFIDENCE = 0.35f;
constexpr float MAX_STALL_CONFIDENCE = 0.60f;

constexpr int MANY_CHECKING_SQUARES = 4;
constexpr float HIGH_CONFIDENCE = 0.80f;
constexpr int CRITICAL_FLAG_TIME_MS = 5000;

struct PieceChoice {
  PieceType piece = NO_PIECE_TYPE;
  int checking_squares = 0;
  int pocket_count = 0;
  int marginal_score = 0;
};

struct StrategySignals {
  bool in_check = false;
  int hostile_drop_checks = 0;
  int available_drop_checks = 0;
  int score = 0;
  bool can_flag = false;
  int flag_target_ms = 0;
  int flag_edge_ms = 0;
};

int checking_drop_count(const DropCheckMasks &checks, const Pocket &pocket) {
  int count = 0;
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    if (pocket.contains(pt))
      count += std::popcount(checks.for_piece(pt));
  }
  return count;
}

PieceChoice choose_piece(const DropCheckMasks &checks, const Pocket &pocket) {
  PieceChoice best;

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    int squares = std::popcount(checks.for_piece(pt));
    if (squares == 0)
      continue;

    int count = pocket.count(pt);

    int score = (squares * PIECE_CHECKING_SQUARE_WEIGHT +
                 PieceValue::effective_value(pt)) /
                (count + 1);

    if (score > best.marginal_score ||
        (score == best.marginal_score &&
         PieceValue::effective_value(pt) >
             PieceValue::effective_value(best.piece))) {
      best = {pt, squares, count, score};
    }
  }

  return best;
}

Bitboard attacks_by(const Board &board, Colour colour) {
  Bitboard attacks =
      pawn_attacks(board.bitboard_piece(make_piece(colour, PAWN)), colour);

  Bitboard occupancy = board.bitboard_all();

  for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
    Bitboard pieces = board.bitboard_piece(make_piece(colour, pt));

    while (pieces) {
      Square square = static_cast<Square>(std::countr_zero(pieces));
      pieces &= pieces - 1;

      attacks |= pt == KING ? king_attacks(square)
                            : piece_attacks(pt, colour, square, occupancy);
    }
  }

  return attacks;
}

int estimate_eta(const BughousePosition &position, PlayerId requester,
                 const PieceChoice &choice) {
  if (choice.pocket_count > 0)
    return ETA_USABLE;

  PlayerId partner = partner_of(requester);
  const Board &partner_board = position.boards[board_of(partner)];
  Colour partner_colour = colour_of(partner);

  Bitboard targets = partner_board.bitboard_piece(
      make_piece(flip(partner_colour), choice.piece));

  int turn_delay = partner_board.sideToMove == partner_colour ? 0 : 1;

  if (targets && (attacks_by(partner_board, partner_colour) & targets))
    return ETA_IMMEDIATE_CAPTURE + turn_delay;

  if (targets)
    return ETA_ON_PARTNER_BOARD + turn_delay;

  return ETA_CURRENTLY_UNAVAILABLE + turn_delay;
}

float piece_confidence(const PieceChoice &choice) {
  float geometry =
      std::min(1.0f, choice.checking_squares / FULL_GEOMETRY_SQUARES);

  float scarcity = 1.0f / (1.0f + POCKET_SCARCITY_FACTOR * choice.pocket_count);

  return std::clamp(geometry * scarcity, 0.0f, 1.0f);
}

StrategySignals
make_strategy_signals(const SearchResult &search_result,
                      const BughousePosition &position, PlayerId root_player,
                      const std::array<int64_t, PLAYER_NO> *remaining_ms) {
  const Board &board = position.boards[board_of(root_player)];
  Colour root_colour = colour_of(root_player);

  DropCheckMasks own_checks = drop_check_masks(board, root_colour);
  DropCheckMasks hostile_checks = drop_check_masks(board, flip(root_colour));

  StrategySignals signals;
  signals.in_check = board.is_in_check();

  signals.available_drop_checks =
      checking_drop_count(own_checks, position.pockets[to_int(root_player)]);

  signals.hostile_drop_checks = checking_drop_count(
      hostile_checks, position.pockets[to_int(next_player(root_player))]);

  signals.score = search_result.score;

  if (remaining_ms) {
    PlayerId partner = partner_of(root_player);
    PlayerId target = next_player(partner);

    int64_t partner_time = (*remaining_ms)[to_int(partner)];
    int64_t target_time = (*remaining_ms)[to_int(target)];
    int64_t edge = partner_time - target_time;

    signals.can_flag = target_time > 0 &&
                       target_time <= FLAG_TARGET_TIME_THRESHOLD_MS &&
                       edge >= FLAG_MIN_TIME_EDGE_MS;

    signals.flag_target_ms =
        static_cast<int>(std::max<int64_t>(0, target_time));

    signals.flag_edge_ms = static_cast<int>(std::max<int64_t>(0, edge));
  }

  return signals;
}

StrategyRequest choose_strategy(const StrategySignals &signals) {
  StrategyRequest request;

  if (signals.in_check || signals.hostile_drop_checks > 0) {
    request.strat = StrategyType::Defend;

    request.confidence =
        signals.in_check
            ? DEFEND_IN_CHECK_CONFIDENCE
            : std::min(MAX_DEFEND_CONFIDENCE,
                       CONFIDENCE_BASE + DROP_CHECK_CONFIDENCE_STEP *
                                             signals.hostile_drop_checks);

    request.urgency = signals.in_check ? Urgency::Critical : Urgency::High;

  } else if (signals.available_drop_checks > 0 ||
             signals.score >= FORCING_SCORE_THRESHOLD) {
    request.strat = StrategyType::AttackNow;

    request.confidence =
        signals.available_drop_checks > 0
            ? std::min(MAX_ATTACK_CONFIDENCE,
                       CONFIDENCE_BASE + DROP_CHECK_CONFIDENCE_STEP *
                                             signals.available_drop_checks)
            : std::min(MAX_SCORE_CONFIDENCE,
                       CONFIDENCE_BASE +
                           (signals.score - FORCING_SCORE_THRESHOLD) /
                               SCORE_CONFIDENCE_SCALE);

    request.urgency =
        request.confidence >= HIGH_CONFIDENCE ? Urgency::High : Urgency::Medium;

  } else if (signals.can_flag) {
    request.strat = StrategyType::Flag;

    float time_pressure =
        1.0f - signals.flag_target_ms /
                   static_cast<float>(FLAG_TARGET_TIME_THRESHOLD_MS);

    float edge_strength =
        std::min(1.0f, signals.flag_edge_ms / FLAG_EDGE_CONFIDENCE_SCALE_MS);

    request.confidence = std::clamp(
        CONFIDENCE_BASE + FLAG_CONFIDENCE_TIME_WEIGHT * time_pressure +
            FLAG_CONFIDENCE_EDGE_WEIGHT * edge_strength,
        0.0f, 1.0f);

    request.urgency = signals.flag_target_ms <= CRITICAL_FLAG_TIME_MS
                          ? Urgency::Critical
                          : Urgency::High;

  } else if (signals.score <= -STRATEGIC_SCORE_THRESHOLD) {
    request.strat = StrategyType::AvoidTrades;

    request.confidence = std::min(
        MAX_SCORE_CONFIDENCE, STRATEGIC_CONFIDENCE_BASE +
                                  (-signals.score - STRATEGIC_SCORE_THRESHOLD) /
                                      SCORE_CONFIDENCE_SCALE);

    request.urgency = Urgency::Medium;

  } else if (signals.score >= STRATEGIC_SCORE_THRESHOLD) {
    request.strat = StrategyType::TradeEverything;

    request.confidence = std::min(
        MAX_SCORE_CONFIDENCE, STRATEGIC_CONFIDENCE_BASE +
                                  (signals.score - STRATEGIC_SCORE_THRESHOLD) /
                                      SCORE_CONFIDENCE_SCALE);

    request.urgency = Urgency::Medium;

  } else {
    request.strat = StrategyType::Stall;

    request.confidence = std::clamp(
        STALL_CONFIDENCE_BASE - std::abs(signals.score) / STALL_SCORE_SCALE,
        MIN_STALL_CONFIDENCE, MAX_STALL_CONFIDENCE);

    request.urgency = Urgency::Low;
  }

  request.confidence = std::clamp(request.confidence, 0.0f, 1.0f);

  return request;
}

Message generate(const SearchResult &search_result,
                 const BughousePosition &position, PlayerId root_player,
                 const std::array<int64_t, PLAYER_NO> *remaining_ms) {
  init_attack_tables();

  Message message;
  message.sender = root_player;

  const Board &board = position.boards[board_of(root_player)];
  const Pocket &pocket = position.pockets[to_int(root_player)];
  message.move_no = board.fullMove;

  DropCheckMasks checks = drop_check_masks(board, colour_of(root_player));
  PieceChoice choice = choose_piece(checks, pocket);
  if (choice.piece != NO_PIECE_TYPE) {
    message.piece_request.piece = choice.piece;
    message.piece_request.confidence = piece_confidence(choice);
    message.piece_request.urgency =
        board.is_in_check()                                ? Urgency::Critical
        : choice.checking_squares >= MANY_CHECKING_SQUARES ? Urgency::High
                                                           : Urgency::Medium;
    message.piece_request.eta_plies =
        estimate_eta(position, root_player, choice);
  }

  message.strat_request = choose_strategy(make_strategy_signals(
      search_result, position, root_player, remaining_ms));
  return message;
}
} // namespace

Message Generator::generate_message(const SearchResult &search_result,
                                    const BughousePosition &position,
                                    PlayerId root_player) const {
  return generate(search_result, position, root_player, nullptr);
}

Message Generator::generate_message(
    const SearchResult &search_result, const BughousePosition &position,
    PlayerId root_player,
    const std::array<int64_t, PLAYER_NO> &remaining_ms) const {
  return generate(search_result, position, root_player, &remaining_ms);
}