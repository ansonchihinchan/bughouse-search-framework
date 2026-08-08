#include "eval/classical/material.h"
#include "game/piece_value.h"
#include <bit>

EvalScore MaterialEvaluator::evaluate(const ClassicalContext &context) const {
  const Board &board = context.board;

  int score = 0;

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    score += PieceValue::PIECE_VALUE[pt] *
             (std::popcount(board.bitboard_piece(make_piece(WHITE, pt))) -
              std::popcount(board.bitboard_piece(make_piece(BLACK, pt))));
  }

  return EvalScore(score);
}