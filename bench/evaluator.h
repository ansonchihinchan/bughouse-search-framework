#pragma once

#include "eval/evaluator.h"
#include "game/bughouse.h"

// Simple material-only evaluator
class BenchEvaluator : public Evaluator {
public:
  int evaluate(const BughousePosition &position, PlayerId root_player,
               const std::array<int64_t, PLAYER_NO> &remaining) const override {
    static constexpr int VALUE[PIECE_TYPE_NO] = {0,   100, 320,  330,
                                                 550, 900, 20000};

    int my_team = to_int(root_player) % 2;
    int score = 0;

    for (int b = 0; b < BOARD_NO; b++) {
      Colour my_colour = (my_team == 0)
                             ? colour_of_player(to_player(2 * b))
                             : colour_of_player(to_player(2 * b + 1));

      const Board &board = position.boards[b];
      for (Square sq = 0; sq < SQUARE_NO; sq++) {
        Piece piece = board.piece_on(sq);
        if (piece.is_empty())
          continue;
        int sign = (piece.colour == my_colour) ? 1 : -1;
        score += sign * VALUE[piece.type];
      }
    }

    for (int p = 0; p < PLAYER_NO; p++) {
      int sign = (p % 2 == my_team) ? 1 : -1;
      const Pocket &pocket = position.pockets[p];
      for (int pt = PAWN; pt <= QUEEN; pt++)
        score += sign * VALUE[pt] * pocket.count(static_cast<PieceType>(pt));
    }

    return score;
  }

  bool is_noisy(const BughousePosition &position,
                PlayerId root_player) const override {
    return true;
  }
};
