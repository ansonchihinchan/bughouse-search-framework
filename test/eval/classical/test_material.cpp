#include <catch2/catch_all.hpp>

#include "eval/classical/material.h"
#include "eval/types.h"
#include "game/bughouse.h"
#include "search/see.h"

TEST_CASE("MaterialEvaluator scores the balanced start position as zero",
          "[eval][material]") {
  BughousePosition pos;
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("MaterialEvaluator credits an extra queen on the root player's own "
          "board",
          "[eval][material]") {
  BughousePosition pos;
  // White (player 0) has an extra queen, board 1 stay balanced
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == SEE::PIECE_VALUE[QUEEN]);
}

TEST_CASE("MaterialEvaluator credits material on the partner's board using "
          "the partner's colour, not the root's colour",
          "[eval][material]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  // Player 0's partner plays Black on board 1
  // Give Black an extra rook there
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/2r1K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == SEE::PIECE_VALUE[ROOK]);
}

TEST_CASE("MaterialEvaluator credits the root player's own pocket with the "
          "pocket bonus",
          "[eval][material]") {
  BughousePosition pos;
  pos.pockets[0].add(KNIGHT);
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() ==
          SEE::PIECE_VALUE[KNIGHT] + SEE::POCKET_BONUS[KNIGHT]);
}

TEST_CASE("MaterialEvaluator credits the partner's pocket exactly like the "
          "root player's own",
          "[eval][material]") {
  BughousePosition pos;
  pos.pockets[2].add(BISHOP);
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() ==
          SEE::PIECE_VALUE[BISHOP] + SEE::POCKET_BONUS[BISHOP]);
}

TEST_CASE("MaterialEvaluator subtracts the opposing team's pocket material",
          "[eval][material]") {
  BughousePosition pos;
  pos.pockets[1].add(ROOK);
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() ==
          -(SEE::PIECE_VALUE[ROOK] + SEE::POCKET_BONUS[ROOK]));
}

TEST_CASE("MaterialEvaluator sums multiple pieces and pockets consistently",
          "[eval][material]") {
  BughousePosition pos;
  // +R +Q for White
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1");
  pos.pockets[0].add(PAWN);
  // cancels out pawn raw value
  pos.pockets[1].add(PAWN);
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);

  int expected = SEE::PIECE_VALUE[ROOK] + SEE::PIECE_VALUE[QUEEN] +
                 (SEE::PIECE_VALUE[PAWN] + SEE::POCKET_BONUS[PAWN]) -
                 (SEE::PIECE_VALUE[PAWN] + SEE::POCKET_BONUS[PAWN]);
  REQUIRE(score.mid_game() == expected);
}

TEST_CASE("MaterialEvaluator is antisymmetric between opposing players",
          "[eval][material]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
  pos.pockets[0].add(PAWN);
  pos.pockets[3].add(BISHOP);
  BughouseClock clock = make_clock();

  MaterialEvaluator eval;

  EvalScore score_player0 =
      eval.evaluate(to_context(pos, make_context(clock, to_player(0))));
  EvalScore score_player1 =
      eval.evaluate(to_context(pos, make_context(clock, to_player(1))));

  REQUIRE(score_player0.mid_game() == -score_player1.mid_game());
  REQUIRE(score_player0.end_game() == -score_player1.end_game());
}

TEST_CASE("MaterialEvaluator gives identical scores for teammates",
          "[eval][material]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
  pos.pockets[0].add(PAWN);
  BughouseClock clock = make_clock();

  MaterialEvaluator eval;

  EvalScore score_player0 =
      eval.evaluate(to_context(pos, make_context(clock, to_player(0))));
  EvalScore score_player2 =
      eval.evaluate(to_context(pos, make_context(clock, to_player(2))));

  REQUIRE(score_player0.mid_game() == score_player2.mid_game());
}