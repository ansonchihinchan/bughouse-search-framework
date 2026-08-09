#include <catch2/catch_all.hpp>

#include "eval/classical/material.h"
#include "eval/types.h"
#include "game/board.h"
#include "search/see.h"

TEST_CASE("MaterialEvaluator scores the balanced start position as zero",
          "[eval][material]") {
  Board board;
  ClassicalContext ctx = make_classical_context(board);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("MaterialEvaluator credits an extra queen on the root player's own "
          "board",
          "[eval][material]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");
  ClassicalContext ctx = make_classical_context(board);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == SEE::PIECE_VALUE[QUEEN]);
}

TEST_CASE("MaterialEvaluator sums pieces consistently", "[eval][material]") {
  Board board;
  // +R +Q for White
  board.load_fen("4k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1");
  ClassicalContext ctx = make_classical_context(board);

  MaterialEvaluator eval;
  EvalScore score = eval.evaluate(ctx);

  int expected = SEE::PIECE_VALUE[ROOK] + SEE::PIECE_VALUE[QUEEN];
  REQUIRE(score.mid_game() == expected);
}