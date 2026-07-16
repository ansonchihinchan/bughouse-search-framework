#include "eval/classical/material.h"
#include "search/see.h"

EvalScore evaluate(const EvalContext &context) {
  Board board = context.position.boards[board_of(context.search.root_player)];
  int score = 0;
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    int white = std::popcount(board.bitboard_piece(make_piece(WHITE, pt)));
    int black = std::popcount(board.bitboard_piece(make_piece(BLACK, pt)));
    score += SEE::PIECE_VALUE[pt] * (white - black);
  }
  return EvalScore(score);
}