#include "eval/bughouse/drop.h"
#include "eval/const.h"
#include "game/attacks.h"
#include "game/movegen.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <vector>

namespace {
Bitboard piece_attack_bb(PieceType pt, Colour colour, Square sq, Bitboard occ) {
  switch (pt) {
  case PAWN:
    return pawn_attacks(1ULL << sq, colour);
  case KNIGHT:
    return knight_attacks(sq);
  case BISHOP:
    return bishop_attacks(sq, occ);
  case ROOK:
    return rook_attacks(sq, occ);
  case QUEEN:
    return bishop_attacks(sq, occ) | rook_attacks(sq, occ);
  default:
    return 0;
  }
}

bool square_gives_check(const Board &board, PieceType pt, Colour colour,
                        Square to) {
  Bitboard enemy_king = board.bitboard_piece(make_piece(flip(colour), KING));
  if (!enemy_king)
    return false;
  Bitboard occ = board.bitboard_all() | (1ULL << to);
  return (piece_attack_bb(pt, colour, to, occ) & enemy_king) != 0;
}

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

bool supports_promotion(const Board &board, Colour colour,
                        Bitboard drop_attacks, Square drop_sq) {
  Bitboard pawns = board.bitboard_piece(make_piece(colour, PAWN));
  int promo_rank = (colour == WHITE) ? 6 : 1;
  Bitboard about_to_promote = pawns & (0xFFULL << (promo_rank * 8));

  while (about_to_promote) {
    Square psq = static_cast<Square>(std::countr_zero(about_to_promote));
    about_to_promote &= about_to_promote - 1;
    Square promo_sq = static_cast<Square>(psq + ((colour == WHITE) ? 8 : -8));

    bool defends_pawn = drop_sq != psq && (drop_attacks & (1ULL << psq));
    bool guards_queening_square =
        drop_sq != promo_sq && (drop_attacks & (1ULL << promo_sq));

    if (defends_pawn || guards_queening_square)
      return true;
  }
  return false;
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

bool is_defensive_square(const Board &board, Colour colour, Square drop_sq) {
  Bitboard king_bb = board.bitboard_piece(make_piece(colour, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  Colour enemy = flip(colour);
  if (!board.is_attacked(ksq, enemy))
    return false;

  Bitboard sliders = board.bitboard_piece(make_piece(enemy, BISHOP)) |
                     board.bitboard_piece(make_piece(enemy, ROOK)) |
                     board.bitboard_piece(make_piece(enemy, QUEEN));
  while (sliders) {
    Square s = static_cast<Square>(std::countr_zero(sliders));
    sliders &= sliders - 1;
    if (on_ray_between(s, ksq, drop_sq))
      return true;
  }
  return false;
}

EvalScore best_drop_threat(const Board &board, PieceType pt, Colour colour,
                           Bitboard enemy_king_zone,
                           const std::vector<Move> &drops) {
  EvalScore best(0);
  bool found = false;

  for (const Move &move : drops) {
    if (move.drop_pt != pt)
      continue;

    Square to = move.to;
    Bitboard occ_after = board.bitboard_all() | (1ULL << to);
    Bitboard attack_bb = piece_attack_bb(pt, colour, to, occ_after);

    int mid = 0, end = 0;

    bool check = square_gives_check(board, pt, colour, to);
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

    int fork = fork_value(board, colour, attack_bb);
    if (fork > 0) {
      mid += fork * DROP_FORK_WEIGHT_MID / 100;
      end += fork * DROP_FORK_WEIGHT_END / 100;
    }

    if (supports_promotion(board, colour, attack_bb, to)) {
      mid += DROP_PROMOTION_SUPPORT_MID;
      end += DROP_PROMOTION_SUPPORT_END;
    }

    if (is_defensive_square(board, colour, to)) {
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

  std::vector<Move> drops = generate_drop_moves(board, pocket);
  if (drops.empty())
    return EvalScore(0);

  EvalScore total(0);
  int active_types = 0;

  for (int pt = PAWN; pt <= QUEEN; pt++) {
    PieceType piece_type = static_cast<PieceType>(pt);
    if (!pocket.contains(piece_type))
      continue;

    EvalScore threat =
        best_drop_threat(board, piece_type, colour, enemy_king_zone, drops);
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

  Colour us = colour_of_player(bughouse.root_player);
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