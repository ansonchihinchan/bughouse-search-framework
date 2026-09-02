#include "eval/bughouse/drop.h"
#include "eval/const.h"
#include "game/attacks.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace {
// return the number of king's flight squares remain uncovered by the drop
int king_escape_squares(const Board &board, Colour king_colour, Square drop_sq,
                        Bitboard drop_attacks) {
  Bitboard king_bb = board.bitboard_piece(make_piece(king_colour, KING));
  if (!king_bb)
    return 8;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard own_pieces = board.bitboard_colour(king_colour);
  Bitboard flight = king_attacks(ksq) & ~own_pieces & ~(1ULL << drop_sq);
  return std::popcount(flight & ~drop_attacks);
}

int fork_value(const Board &board, Colour attacker_colour, Bitboard attack_bb) {
  Colour enemy = flip(attacker_colour);
  int hits = 0;
  int value = 0;
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    int count =
        std::popcount(attack_bb & board.bitboard_piece(make_piece(enemy, pt)));
    hits += count;
    value += count * PieceValue::PIECE_VALUE[pt];
  }
  return hits >= 2 ? value : 0;
}

Bitboard promotion_support_targets(const Board &board, Colour colour) {
  Bitboard pawns = board.bitboard_piece(make_piece(colour, PAWN));
  int promo_rank = (colour == WHITE) ? 6 : 1;
  Bitboard about_to_promote = pawns & (0xFFULL << (promo_rank * 8));
  Bitboard promotion_squares =
      colour == WHITE ? about_to_promote << 8 : about_to_promote >> 8;
  return about_to_promote | promotion_squares;
}

bool on_ray_between(Square a, Square b, Square sq) {
  int fa = file_of(a), ra = rank_of(a);
  int fb = file_of(b), rb = rank_of(b);
  int fs = file_of(sq), rs = rank_of(sq);

  int df = fb - fa, dr = rb - ra;
  if (df != 0 && dr != 0 && std::abs(df) != std::abs(dr))
    return false;

  int steps = std::max(std::abs(df), std::abs(dr));
  if (steps < 2)
    return false;

  int sf = (df == 0) ? 0 : df / std::abs(df);
  int sr = (dr == 0) ? 0 : dr / std::abs(dr);

  for (int i = 1; i < steps; i++)
    if (fa + sf * i == fs && ra + sr * i == rs)
      return true;

  return false;
}

Bitboard defensive_drop_squares(const Board &board, Colour colour,
                                Bitboard candidates) {
  Bitboard king_bb = board.bitboard_piece(make_piece(colour, KING));
  if (!king_bb)
    return 0;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  Colour enemy = flip(colour);
  if (!board.is_attacked(ksq, enemy))
    return 0;

  Bitboard sliders = board.bitboard_piece(make_piece(enemy, BISHOP)) |
                     board.bitboard_piece(make_piece(enemy, ROOK)) |
                     board.bitboard_piece(make_piece(enemy, QUEEN));
  Bitboard defensive = 0;
  while (sliders) {
    Square s = static_cast<Square>(std::countr_zero(sliders));
    sliders &= sliders - 1;
    Bitboard remaining = candidates;
    while (remaining) {
      Square drop_sq = static_cast<Square>(std::countr_zero(remaining));
      remaining &= remaining - 1;
      if (on_ray_between(s, ksq, drop_sq))
        defensive |= 1ULL << drop_sq;
    }
  }
  return defensive;
}

EvalScore best_drop_threat(const Board &board, PieceType pt, Colour colour,
                           Bitboard enemy_king_zone, Bitboard candidates,
                           Bitboard checking_squares,
                           Bitboard promotion_targets,
                           Bitboard defensive_squares, bool can_fork) {
  EvalScore best(0);
  bool found = false;
  Bitboard occ = board.bitboard_all();

  while (candidates) {
    Square to = static_cast<Square>(std::countr_zero(candidates));
    candidates &= candidates - 1;
    Bitboard to_bb = 1ULL << to;
    Bitboard occ_after = occ | to_bb;
    Bitboard attack_bb = piece_attacks(pt, colour, to, occ_after);

    int mid = 0, end = 0;

    bool check = (checking_squares & to_bb) != 0;
    if (check) {
      mid += DROP_CHECK_BONUS_MID;
      end += DROP_CHECK_BONUS_END;

      if (king_escape_squares(board, flip(colour), to, attack_bb) <= 1) {
        mid += DROP_MATING_NET_BONUS_MID;
        end += DROP_MATING_NET_BONUS_END;
      }
    } else if (attack_bb & enemy_king_zone) {
      mid += DROP_KING_PROXIMITY_MID;
      end += DROP_KING_PROXIMITY_END;
    }

    int fork = can_fork ? fork_value(board, colour, attack_bb) : 0;
    if (fork > 0) {
      mid += fork * DROP_FORK_WEIGHT_MID / 100;
      end += fork * DROP_FORK_WEIGHT_END / 100;
    }

    if (attack_bb & (promotion_targets & ~to_bb)) {
      mid += DROP_PROMOTION_SUPPORT_MID;
      end += DROP_PROMOTION_SUPPORT_END;
    }

    if (defensive_squares & to_bb) {
      mid += DROP_DEFENSE_BONUS_MID;
      end += DROP_DEFENSE_BONUS_END;
    }

    if (mid == 0 && end == 0)
      continue;

    if (!found || mid + end > best.mid_game() + best.end_game()) {
      best = EvalScore(mid, end);
      found = true;
    }
  }

  return best;
}

EvalScore pocket_drop_threat(const Board &board, const Pocket &pocket,
                             Colour colour, Bitboard enemy_king_zone) {
  if (pocket.empty())
    return EvalScore(0);

  Bitboard empty = ~board.bitboard_all();
  if (!empty)
    return EvalScore(0);

  EvalScore total(0);
  int active_types = 0;
  Bitboard promotion_targets = promotion_support_targets(board, colour);
  Bitboard defensive_squares = defensive_drop_squares(board, colour, empty);
  DropCheckMasks checking = drop_check_masks(board, colour);
  Bitboard enemy_non_king =
      board.bitboard_colour(flip(colour)) &
      ~board.bitboard_piece(make_piece(flip(colour), KING));
  bool can_fork = std::popcount(enemy_non_king) >= 2;

  for (int pt = PAWN; pt <= QUEEN; pt++) {
    PieceType piece_type = static_cast<PieceType>(pt);
    if (!pocket.contains(piece_type))
      continue;

    Bitboard candidates = empty;
    if (piece_type == PAWN)
      candidates &= ~0xFF000000000000FFULL;
    EvalScore threat =
        best_drop_threat(board, piece_type, colour, enemy_king_zone, candidates,
                         checking.for_piece(piece_type), promotion_targets,
                         defensive_squares, can_fork);
    if (threat.mid_game() != 0 || threat.end_game() != 0) {
      total += threat;
      active_types++;
    }
  }

  if (active_types > 1)
    total += EvalScore(DROP_FLEXIBILITY_BONUS_MID * (active_types - 1),
                       DROP_FLEXIBILITY_BONUS_END * (active_types - 1));

  return total;
}

} // namespace

EvalScore DropEvaluator::evaluate(const EvalContext &context) const {
  const BughouseContext &bughouse = context.bughouse;
  const ClassicalContext &classical = context.classical;
  const Board &board = classical.board;

  Colour us = colour_of(bughouse.root_player);
  Colour them = flip(us);

  EvalScore our_threats = pocket_drop_threat(
      board, bughouse.own_pocket(), us, classical.attack_info.kingZone[them]);
  EvalScore their_threats = pocket_drop_threat(
      board, bughouse.opp_pocket(), them, classical.attack_info.kingZone[us]);

  EvalScore score = our_threats - their_threats;

  if (!bughouse.partner_pocket().empty()) {
    int urgency =
        std::clamp(static_cast<int>(context.communication.partner.king_danger),
                   0, PARTNER_KING_DANGER_CLAMP);
    score += EvalScore(
        DROP_PARTNER_ESTIMATE_WEIGHT_MID * urgency / PARTNER_KING_DANGER_CLAMP,
        DROP_PARTNER_ESTIMATE_WEIGHT_END * urgency / PARTNER_KING_DANGER_CLAMP);
  }

  return score;
}