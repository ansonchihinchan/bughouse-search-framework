#pragma once

#include "eval/evaluator.h"
#include "game/bughouse.h"

// Deterministic material-only evaluator used solely by the benchmark
// harness. It is intentionally simple (no positional terms, no time-based
// terms from the clock) so that every search algorithm under benchmark sees
// identical, cheap-to-compute scores -- keeping node counts and timings
// comparable across algorithms and across runs.
class MaterialEvaluator : public Evaluator {
public:
  int evaluate(const BughousePosition &position,
              const SearchContext &context) const override {
    static constexpr int VALUE[PIECE_TYPE_NO] = {0,   100, 320, 330,
                                                 550, 900, 20000};

    int my_team = to_int(context.root_player) % 2;
    int score = 0;

    for (int b = 0; b < BOARD_NO; b++) {
      // Player index 2b always belongs to team 0, 2b+1 to team 1 -- see
      // bughouse.h's partner_of()/board_of() layout.
      Colour my_colour = (my_team == 0) ? colour_of_player(to_player(2 * b))
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

  // Always route leaves through quiescence search so capture sequences are
  // resolved before scoring -- exercising the same leaf path a real
  // evaluator would use, and keeping node counts meaningful.
  bool is_noisy(const BughousePosition &, const SearchContext &) const override {
    return true;
  }
};
