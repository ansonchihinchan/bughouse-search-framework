#include "eval/classical/mobility.h"
#include "game/attacks.h"
#include <bit>

namespace {
constexpr int MOBILITY_WEIGHT[PIECE_TYPE_NO] = {0, 0, 4, 3, 2, 1, 0};

Bitboard piece_mobility(const Board &board, Piece piece, Square sq) {
  Bitboard all = board.bitboard_all();
  Bitboard own = board.bitboard_colour(piece.colour);
  switch (piece.type) {
  case KNIGHT:
    return knight_attacks(sq) & ~own;
  case BISHOP:
    return bishop_attacks(sq, all) & ~own;
  case ROOK:
    return rook_attacks(sq, all) & ~own;
  case QUEEN:
    return (bishop_attacks(sq, all) | rook_attacks(sq, all)) & ~own;
  default:
    return 0;
  }
}

int mobility_score(const Board &board, Colour colour) {
  int score = 0;
  for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN}) {
    Bitboard pieces = board.bitboard_piece(make_piece(colour, pt));
    while (pieces) {
      Square sq = static_cast<Square>(std::countr_zero(pieces));
      pieces &= pieces - 1;
      int count =
          std::popcount(piece_mobility(board, make_piece(colour, pt), sq));
      score += MOBILITY_WEIGHT[pt] * count;
    }
  }
  return score;
}
} // namespace

EvalScore MobilityEvaluator::evaluate(const EvalContext &context) const {
  const Board &board = context.board;

  return EvalScore(mobility_score(board, WHITE) - mobility_score(board, BLACK));
}