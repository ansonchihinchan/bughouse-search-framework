#include <catch2/catch_all.hpp>

#include "eval/bughouse/pocket.h"
#include "eval/const.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("PocketEvaluator scores all-empty pockets as zero regardless of "
          "board",
          "[eval][bughouse][pocket]") {
  Fixture fx(BARE_KINGS);
  PocketEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PocketEvaluator values a pocketed knight relative to fully-open, "
          "king-safe conditions",
          "[eval][bughouse][pocket]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[0].add(KNIGHT);

  PocketEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 356);
  REQUIRE(score.end_game() == 372);
}

TEST_CASE("PocketEvaluator credits a pocketed knight more when the opponent's "
          "king is exposed",
          "[eval][bughouse][pocket]") {
  Fixture fx("4k3/8/8/8/8/8/8/K3R3 w - - 0 1");
  fx.pockets[0].add(KNIGHT);

  PocketEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 364);
  REQUIRE(score.end_game() == 376);
}

TEST_CASE("PocketEvaluator discounts partner's pocket by the confidence "
          "divisor and uses the neutral openness midpoint",
          "[eval][bughouse][pocket]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[2].add(QUEEN);
  fx.partner.king_danger = 10.f;

  PocketEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 555);
  REQUIRE(score.end_game() == 550);
}

TEST_CASE("PocketEvaluator treats the opponent's partner's pocket as plain "
          "material with no board context",
          "[eval][bughouse][pocket]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[3].add(ROOK);

  PocketEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -320);
  REQUIRE(score.end_game() == -320);
}