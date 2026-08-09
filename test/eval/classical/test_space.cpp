#include <catch2/catch_all.hpp>

#include "eval/classical/space.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("SpaceEvaluator scores zero for bare kings", "[eval][space]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  SpaceEvaluator eval;
  REQUIRE(eval.evaluate(make_classical_context(board)).mid_game() == 0);
}

TEST_CASE("SpaceEvaluator scores zero when fewer than two minor/major pieces "
          "remain, even if squares are attacked",
          "[eval][space]") {
  Board board;
  // King + single knight
  board.load_fen("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1");

  SpaceEvaluator eval;
  REQUIRE(eval.evaluate(make_classical_context(board)).mid_game() == 0);
}

TEST_CASE("SpaceEvaluator rewards controlling squares in one's own territory "
          "once the minor/major-piece threshold is met",
          "[eval][space]") {
  Board board;
  // Two knights reaching into the White ranks 2-3 zone
  // Plus king's own contribution to that zone
  board.load_fen("4k3/8/8/8/8/8/8/1N2K1N1 w - - 0 1");

  SpaceEvaluator eval;
  // Knights on b1/g1 control {a3,c3,d2} and {e2,f3,h3}
  // King on e1 controls {d2,e2,f2}
  REQUIRE(eval.evaluate(make_classical_context(board)).mid_game() == 14);
}