#include "eval/classical/material.h"
#include "game/bughouse.h"
#include "search/see.h"
#include <bit>

EvalScore MaterialEvaluator::evaluate(const EvalContext &context) const {
  const SearchContext &search = context.search;
  const BughousePosition &position = context.position;
  PlayerId root = search.root_player;

  int score = 0;

  for (int b = 0; b < BOARD_NO; b++) {
    const Board &board = position.boards[b];

    Colour ours = team_colour(root, b);
    Colour theirs = flip(ours);

    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
      score += SEE::PIECE_VALUE[pt] *
               (std::popcount(board.bitboard_piece(make_piece(ours, pt))) -
                std::popcount(board.bitboard_piece(make_piece(theirs, pt))));
    }
  }

  for (int p = 0; p < PLAYER_NO; p++) {
    int sign = team_sign(root, to_player(p));
    const Pocket &pocket = position.pockets[p];
    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
      int count = pocket.count(pt);
      if (count > 0)
        score += sign * count * (SEE::PIECE_VALUE[pt] + SEE::POCKET_BONUS[pt]);
    }
  }

  return EvalScore(score);
}