#include "eval/classical/material.h"
#include "search/see.h"
#include <bit>

EvalScore MaterialEvaluator::evaluate(const EvalContext &context) const {
  const Board &board = context.board;

  int score = 0;

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    score += SEE::PIECE_VALUE[pt] *
             (std::popcount(board.bitboard_piece(make_piece(WHITE, pt))) -
              std::popcount(board.bitboard_piece(make_piece(BLACK, pt))));
  }

  return EvalScore(score);
}