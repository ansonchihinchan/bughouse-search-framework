#include "eval/classical/activity.h"
#include "eval/const.h"
#include "game/bitboards.h"
#include <bit>

namespace {
constexpr Bitboard WHITE_MINOR_HOME =
    (1ULL << to_square(1, 0)) | (1ULL << to_square(2, 0)) |
    (1ULL << to_square(5, 0)) | (1ULL << to_square(6, 0));
constexpr Bitboard BLACK_MINOR_HOME =
    (1ULL << to_square(1, 7)) | (1ULL << to_square(2, 7)) |
    (1ULL << to_square(5, 7)) | (1ULL << to_square(6, 7));

int activity_score(const Board &board, Colour colour) {
  int score = 0;

  Bitboard minor = board.bitboard_piece(make_piece(colour, KNIGHT)) |
                   board.bitboard_piece(make_piece(colour, BISHOP));
  Bitboard home = (colour == WHITE) ? WHITE_MINOR_HOME : BLACK_MINOR_HOME;
  score -= UNDEVELOPED_PENALTY * std::popcount(minor & home);

  Bitboard own_pawns = board.bitboard_piece(make_piece(colour, PAWN));
  Bitboard opp_pawns = board.bitboard_piece(make_piece(flip(colour), PAWN));
  Bitboard rooks = board.bitboard_piece(make_piece(colour, ROOK));
  while (rooks) {
    Square sq = static_cast<Square>(std::countr_zero(rooks));
    rooks &= rooks - 1;
    Bitboard file = Bitboards::file_mask(file_of(sq));
    if (!(file & own_pawns))
      score += (file & opp_pawns) ? SEMI_OPEN_FILE_BONUS : OPEN_FILE_BONUS;
  }

  return score;
}
} // namespace

EvalScore ActivityEvaluator::evaluate(const ClassicalContext &context) const {
  const Board &board = context.board;

  return EvalScore(activity_score(board, WHITE) - activity_score(board, BLACK));
}