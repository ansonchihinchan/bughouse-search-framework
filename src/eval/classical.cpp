#include "eval/classical.h"
#include "eval/classical/activity.h"
#include "eval/classical/king_safety.h"
#include "eval/classical/material.h"
#include "eval/classical/mobility.h"
#include "eval/classical/pawn.h"
#include "eval/classical/piece_square.h"
#include "eval/classical/space.h"
#include "eval/classical/tempo.h"
#include "game/attacks.h"
#include "game/movegen.h"

ClassicalEvaluator::ClassicalEvaluator() {
  features_.push_back(std::make_unique<MaterialEvaluator>());
  features_.push_back(std::make_unique<PieceSquareEvaluator>());
  features_.push_back(std::make_unique<MobilityEvaluator>());
  features_.push_back(std::make_unique<PawnEvaluator>());
  features_.push_back(std::make_unique<KingSafetyEvaluator>());
  features_.push_back(std::make_unique<SpaceEvaluator>());
  features_.push_back(std::make_unique<TempoEvaluator>());
  features_.push_back(std::make_unique<ActivityEvaluator>());
}

int ClassicalEvaluator::evaluate(const BughousePosition &position,
                                 const SearchContext &search_context) const {
  EvalContext eval_context = to_context(position, search_context);
  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  int white_score = score.final(eval_context.phase);
  return team_colour(search_context.root_player,
                     board_of(search_context.root_player)) == WHITE
             ? white_score
             : -white_score;
}

namespace {
bool drop_gives_check(const Board &board, PieceType pt, Square to,
                      Colour colour) {
  Colour enemy = flip(colour);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard occ = board.bitboard_all();

  switch (pt) {
  case KNIGHT:
    return (knight_attacks(to) & king_bb) != 0;
  case BISHOP:
    return (bishop_attacks(to, occ) & king_bb) != 0;
  case ROOK:
    return (rook_attacks(to, occ) & king_bb) != 0;
  case QUEEN:
    return ((bishop_attacks(to, occ) | rook_attacks(to, occ)) & king_bb) != 0;
  case PAWN: {
    int file_diff = std::abs(file_of(ksq) - file_of(to));
    int rank_diff = rank_of(ksq) - rank_of(to);
    int expected_rank_diff = (colour == WHITE) ? 1 : -1;
    return file_diff == 1 && rank_diff == expected_rank_diff;
  }
  default:
    return false;
  }
}
} // namespace

// in check, captures, drop gives check
bool ClassicalEvaluator::is_noisy(const BughousePosition &position,
                                  const SearchContext &search_context) const {
  const Board &board = position.boards[board_of(search_context.root_player)];

  if (board.is_in_check())
    return true;

  const Pocket &pocket = position.pockets[to_int(search_context.root_player)];
  auto moves = generate_pseudo_legal_moves(board, &pocket);

  for (const Move &m : moves) {
    if (board.is_capture(m))
      return true;
    if (m.is_drop() &&
        drop_gives_check(board, m.drop_pt, m.to, board.sideToMove))
      return true;
  }
  return false;
}