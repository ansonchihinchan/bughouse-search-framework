#include <catch2/catch_all.hpp>

#include "eval/bughouse/drop.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("DropEvaluator scores empty pockets as zero",
          "[eval][bughouse][drop]") {
  Fixture fx(BARE_KINGS);
  DropEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("DropEvaluator credits a knight drop that gives check without a "
          "mating net",
          "[eval][bughouse][drop]") {
  Fixture fx("RRRRkRRR/RR1RRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/"
             "RRRRRRRR/KRRRRRRR w - - 0 1");
  fx.pockets[0].add(KNIGHT);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 25);
  REQUIRE(score.end_game() == 40);
}

TEST_CASE("DropEvaluator adds a mating-net bonus when the checked king has "
          "no flight squares",
          "[eval][bughouse][drop]") {
  Fixture fx("kpRRRRRR/pp1RRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/"
             "RRRRRRRR/RRRRRRRK w - - 0 1");
  fx.pockets[0].add(KNIGHT);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 85);
  REQUIRE(score.end_game() == 130);
}

TEST_CASE("DropEvaluator credits a drop that reaches the enemy king zone "
          "without giving check",
          "[eval][bughouse][drop]") {
  Fixture fx("kRR1RRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/"
             "RRRRRRRR/RRRRRRRK w - - 0 1");
  fx.pockets[0].add(KNIGHT);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 10);
  REQUIRE(score.end_game() == 6);
}

TEST_CASE("DropEvaluator credits a fork hitting two enemy pieces at once",
          "[eval][bughouse][drop]") {
  Fixture fx("kRRRRRRR/RRRnRRRR/RRbRRRRR/RRRR1RRR/RRRRRRRR/RRRRRRRR/"
             "RRRRRRRR/RRRRRRRK w - - 0 1");
  fx.pockets[0].add(KNIGHT);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 227);
  REQUIRE(score.end_game() == 292);
}

TEST_CASE("DropEvaluator credits a rook drop that defends a pawn one step "
          "from promotion",
          "[eval][bughouse][drop]") {
  Fixture fx("RRRRRRRk/RPRRRRRR/R1RRRRRR/R1RRRRRR/R1RRRRRR/R1RRRRRR/"
             "R1RRRRRR/K1RRRRRR w - - 0 1");
  fx.pockets[0].add(ROOK);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 15);
  REQUIRE(score.end_game() == 45);
}

TEST_CASE("DropEvaluator credits a pawn drop that blocks a check against our "
          "own king",
          "[eval][bughouse][drop]") {
  Fixture fx("kRRRrRRR/RRRR1RRR/RRRR1RRR/RRRR1RRR/RRRR1RRR/RRRR1RRR/"
             "RRRR1RRR/RRRRKRRR w - - 0 1");
  fx.pockets[0].add(PAWN);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 20);
  REQUIRE(score.end_game() == 15);
}

TEST_CASE("DropEvaluator adds a flexibility bonus when more than one pocket "
          "piece type has a productive drop",
          "[eval][bughouse][drop]") {
  Fixture fx("kRR1RRRR/RRRRRRRR/RR1RRRRR/RRRRRRRR/RRRRRRRR/RRRRRRRR/"
             "RRRRRRRR/RRRRRRRK w - - 0 1");
  fx.pockets[0].add(KNIGHT);
  fx.pockets[0].add(BISHOP);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 28);
  REQUIRE(score.end_game() == 17);
}

TEST_CASE("DropEvaluator subtracts the opponent's drop threats against our "
          "own king",
          "[eval][bughouse][drop]") {
  Fixture fx("rrrrKrrr/rr1rrrrr/rrrrrrrr/rrrrrrrr/rrrrrrrr/rrrrrrrr/"
             "rrrrrrrr/krrrrrrr w - - 0 1");
  fx.pockets[1].add(KNIGHT);

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -25);
  REQUIRE(score.end_game() == -40);
}

TEST_CASE("DropEvaluator applies a flat partner-pocket estimate scaled by "
          "partner's king danger",
          "[eval][bughouse][drop]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[2].add(QUEEN);
  fx.partner.king_danger = 10.f;

  DropEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 10);
  REQUIRE(score.end_game() == 15);
}