#include "eval/bughouse/king_danger.h"
#include "eval/const.h"
#include "game/attacks.h"

#include <algorithm>
#include <bit>
#include <cmath>

namespace {

struct DropCheckCounts {
  int pawn = 0;
  int knight = 0;
  int bishop = 0;
  int rook = 0;
  int queen = 0;
};

DropCheckCounts compute_drop_check_squares(const Board &board,
                                           Colour attacker_colour) {
  DropCheckCounts counts;
  DropCheckMasks masks = drop_check_masks(board, attacker_colour);

  counts.pawn = std::popcount(masks.pawn);
  counts.knight = std::popcount(masks.knight);
  counts.bishop = std::popcount(masks.bishop);
  counts.rook = std::popcount(masks.rook);
  counts.queen = std::popcount(masks.queen);

  return counts;
}

float boxed_in_multiplier(const Board &board, Square ksq, Colour king_colour,
                          float weight) {
  Bitboard neighbours = king_attacks(ksq);
  int total = std::popcount(neighbours);
  if (total == 0)
    return 1.0f;

  Bitboard blocked_by_own = neighbours & board.bitboard_colour(king_colour);
  float boxed_fraction =
      static_cast<float>(std::popcount(blocked_by_own)) / total;

  return 1.0f + weight * boxed_fraction;
}

int weighted_squares(int square_count, PieceType pt, const Pocket &pocket,
                     const int weight_table[PIECE_TYPE_NO]) {
  if (square_count == 0 || !pocket.contains(pt))
    return 0;

  int capped = std::min(square_count, KING_DANGER_CHECK_SQUARE_CAP);
  int extra_copies = std::min(std::max(0, pocket.count(pt) - 1), 2);
  float copy_multiplier = 1.0f + KING_DANGER_EXTRA_COPY_BONUS * extra_copies;

  return static_cast<int>(
      std::lround(capped * weight_table[pt] * copy_multiplier));
}

EvalScore king_drop_danger(const Board &board, Square ksq, Colour king_colour,
                           Colour attacker_colour,
                           const Pocket &attacker_pocket) {
  DropCheckCounts counts = compute_drop_check_squares(board, attacker_colour);

  int mid = 0, end = 0;
  for (auto [count, pt] :
       {std::pair{counts.pawn, PAWN}, std::pair{counts.knight, KNIGHT},
        std::pair{counts.bishop, BISHOP}, std::pair{counts.rook, ROOK},
        std::pair{counts.queen, QUEEN}}) {
    mid += weighted_squares(count, pt, attacker_pocket, KING_DANGER_WEIGHT_MID);
    end += weighted_squares(count, pt, attacker_pocket, KING_DANGER_WEIGHT_END);
  }

  float box_mid =
      boxed_in_multiplier(board, ksq, king_colour, KING_DANGER_BOX_WEIGHT_MID);
  float box_end =
      boxed_in_multiplier(board, ksq, king_colour, KING_DANGER_BOX_WEIGHT_END);

  return EvalScore(static_cast<int>(std::lround(mid * box_mid)),
                   static_cast<int>(std::lround(end * box_end)));
}

} // namespace

EvalScore KingDangerEvaluator::evaluate(const EvalContext &context) const {
  const Board &board = context.classical.board;
  Colour us = colour_of(context.bughouse.root_player);
  Colour them = flip(us);

  EvalScore danger_to_us(0);
  Bitboard our_king_bb = board.bitboard_piece(make_piece(us, KING));
  if (our_king_bb) {
    Square our_ksq = static_cast<Square>(std::countr_zero(our_king_bb));
    danger_to_us = king_drop_danger(board, our_ksq, us, them,
                                    context.bughouse.opp_pocket());
  }

  EvalScore danger_to_them(0);
  Bitboard their_king_bb = board.bitboard_piece(make_piece(them, KING));
  if (their_king_bb) {
    Square their_ksq = static_cast<Square>(std::countr_zero(their_king_bb));
    danger_to_them = king_drop_danger(board, their_ksq, them, us,
                                      context.bughouse.own_pocket());
  }

  return danger_to_them - danger_to_us;
}