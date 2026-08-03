#include <catch2/catch_all.hpp>

#include "eval/classical.h"
#include "game/bughouse.h"

TEST_CASE("ClassicalEvaluator honours the team-antisymmetry contract: "
          "opposing players see exactly opposite scores",
          "[eval][classical][integration]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1");  // White +Q
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/2r1K3 w - - 0 1"); // Black +R
  pos.pockets[0].add(KNIGHT);
  pos.pockets[3].add(PAWN);
  BughouseClock clock = make_clock();
  ClassicalEvaluator eval;

  int score0 = eval.evaluate(pos, make_context(clock, to_player(0)));
  int score1 = eval.evaluate(pos, make_context(clock, to_player(1)));
  int score2 = eval.evaluate(pos, make_context(clock, to_player(2)));
  int score3 = eval.evaluate(pos, make_context(clock, to_player(3)));

  REQUIRE(score0 == -score1);
  REQUIRE(score2 == -score3);
}

TEST_CASE("ClassicalEvaluator produces a non-zero, materially-sensible score "
          "for a lopsided position",
          "[eval][classical][integration]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1"); // White +R +Q
  BughouseClock clock = make_clock();
  ClassicalEvaluator eval;

  int score = eval.evaluate(pos, make_context(clock, to_player(0)));
  REQUIRE(score > 0);

  int opponent_score = eval.evaluate(pos, make_context(clock, to_player(1)));
  REQUIRE(opponent_score < 0);
}

TEST_CASE("ClassicalEvaluator combines multiple features additively (sanity "
          "check against a hand-picked, materially dominant position)",
          "[eval][classical][integration]") {
  BughousePosition pos;

  pos.boards[0].load_fen("r3k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1");
  BughouseClock clock = make_clock();
  ClassicalEvaluator eval;

  int score = eval.evaluate(pos, make_context(clock, to_player(0)));
  REQUIRE(score > 500); // comfortably more than a single minor piece's worth
}