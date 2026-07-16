#include "eval/classical/activity.h"
#include <bit>

namespace {
constexpr int UNDEVELOPED_PENALTY = 15;
constexpr int OPEN_FILE_BONUS = 15;
constexpr int SEMI_OPEN_FILE_BONUS = 8;

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
Bitboard file_mask(int file) { return FILE_A_BB << file; }

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
    Bitboard file = file_mask(file_of(sq));
    if (!(file & own_pawns))
      score += (file & opp_pawns) ? SEMI_OPEN_FILE_BONUS : OPEN_FILE_BONUS;
  }

  return score;
}
} // namespace

EvalScore ActivityEvaluator::evaluate(const EvalContext &context) const {
  int score = 0;
  for (int b = 0; b < BOARD_NO; b++) {
    const Board &board = context.position.boards[b];
    Colour ours = team_colour(context.search.root_player, b);
    score += activity_score(board, ours) - activity_score(board, flip(ours));
  }
  return EvalScore(score);
}