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
#include <bit>

namespace {
bool has_pseudo_legal_capture(const Board &board) {
  Colour side = board.sideToMove;
  Bitboard enemy = board.bitboard_colour(flip(side));
  Bitboard occ = board.bitboard_all();

  Bitboard pawns = board.bitboard_piece(make_piece(side, PAWN));
  Bitboard pawn_targets = enemy;
  if (board.enPassantSquare >= 0)
    pawn_targets |= 1ULL << board.enPassantSquare;
  if (pawn_attacks(pawns, side) & pawn_targets)
    return true;

  Bitboard knights = board.bitboard_piece(make_piece(side, KNIGHT));
  while (knights) {
    Square from = static_cast<Square>(std::countr_zero(knights));
    knights &= knights - 1;
    if (knight_attacks(from) & enemy)
      return true;
  }

  Bitboard diagonal = board.bitboard_piece(make_piece(side, BISHOP)) |
                      board.bitboard_piece(make_piece(side, QUEEN));
  while (diagonal) {
    Square from = static_cast<Square>(std::countr_zero(diagonal));
    diagonal &= diagonal - 1;
    if (bishop_attacks(from, occ) & enemy)
      return true;
  }

  Bitboard orthogonal = board.bitboard_piece(make_piece(side, ROOK)) |
                        board.bitboard_piece(make_piece(side, QUEEN));
  while (orthogonal) {
    Square from = static_cast<Square>(std::countr_zero(orthogonal));
    orthogonal &= orthogonal - 1;
    if (rook_attacks(from, occ) & enemy)
      return true;
  }

  Bitboard king = board.bitboard_piece(make_piece(side, KING));
  return king &&
         (king_attacks(static_cast<Square>(std::countr_zero(king))) & enemy);
}
} // namespace

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

int ClassicalEvaluator::evaluate(const Board &board, Colour side) const {
  return evaluate(make_classical_context(board), side);
}

int ClassicalEvaluator::evaluate(const ClassicalContext &eval_context,
                                 Colour side) const {
  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  int white_score = score.final(eval_context.phase);
  return side == WHITE ? white_score : -white_score;
}

// in check, captures
bool ClassicalEvaluator::is_noisy(const Board &board) const {
  if (board.is_in_check())
    return true;

  return has_pseudo_legal_capture(board);
}
