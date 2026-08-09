#include <catch2/catch_all.hpp>

#include "eval/bughouse/king_danger.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("KingDangerEvaluator scores empty pockets as zero danger both ways",
          "[eval][bughouse][king_danger]") {
  Fixture fx("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  KingDangerEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("KingDangerEvaluator penalises a knight-drop check square held in "
          "the opponent's pocket",
          "[eval][bughouse][king_danger]") {
  Fixture fx("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  fx.pockets[1].add(KNIGHT);

  KingDangerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -40);
  REQUIRE(score.end_game() == -24);
}

TEST_CASE("KingDangerEvaluator scales the threat up when the king's own "
          "pieces block its escape squares",
          "[eval][bughouse][king_danger]") {
  Fixture fx("4k3/8/8/8/8/8/3PPP2/4K3 w - - 0 1");
  fx.pockets[1].add(KNIGHT);

  KingDangerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -54);
  REQUIRE(score.end_game() == -28);
}

TEST_CASE("KingDangerEvaluator escalates danger for repeated copies of the "
          "same threatening piece",
          "[eval][bughouse][king_danger]") {
  Fixture fx("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  fx.pockets[1].add(KNIGHT);
  fx.pockets[1].add(KNIGHT);
  fx.pockets[1].add(KNIGHT);
  KingDangerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -60);
  REQUIRE(score.end_game() == -36);
}