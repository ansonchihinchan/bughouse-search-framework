#include "eval/classical/piece_square.h"
#include "eval/classical/pst.h"

namespace {
int pst_index(Colour colour, Square sq) {
  int file = file_of(sq);
  int rank = rank_of(sq);
  int effective_rank = (colour == WHITE) ? rank : 7 - rank;
  int row = 7 - effective_rank;
  return row * 8 + file;
}

int pst_lookup(const int table[64], Colour colour, Square sq) {
  return table[pst_index(colour, sq)];
}
} // namespace

EvalScore
PieceSquareEvaluator::evaluate(const ClassicalContext &context) const {
  const Board &board = context.board;
  EvalScore score = EvalScore(0);

  for (Square sq = 0; sq < SQUARE_NO; sq++) {
    Piece piece = board.piece_on(sq);
    if (piece.is_empty())
      continue;

    EvalScore value = EvalScore(0);
    switch (piece.type) {
    case PAWN:
      value = EvalScore(pst_lookup(PAWN_PST, piece.colour, sq));
      break;
    case KNIGHT:
      value = EvalScore(pst_lookup(KNIGHT_PST, piece.colour, sq));
      break;
    case BISHOP:
      value = EvalScore(pst_lookup(BISHOP_PST, piece.colour, sq));
      break;
    case ROOK:
      value = EvalScore(pst_lookup(ROOK_PST, piece.colour, sq));
      break;
    case QUEEN:
      value = EvalScore(pst_lookup(QUEEN_PST, piece.colour, sq));
      break;
    case KING: {
      int mid = pst_lookup(KING_MID_PST, piece.colour, sq);
      int end = pst_lookup(KING_END_PST, piece.colour, sq);
      value = EvalScore(mid, end);
      break;
    }
    default:
      break;
    }
    score = (piece.colour == WHITE) ? score + value : score - value;
  }

  return score;
}