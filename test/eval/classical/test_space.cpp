#include <catch2/catch_all.hpp>

#include "eval/classical/space.h"
#include "eval/types.h"
#include "game/bughouse.h"

TEST_CASE("SpaceEvaluator scores zero when fewer than two minor/major pieces "
          "remain, even if squares are attacked",
          "[eval][space]") {
  BughousePosition pos;
  // King + single knight
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  SpaceEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("SpaceEvaluator scores zero for bare kings", "[eval][space]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  SpaceEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("SpaceEvaluator rewards controlling squares in one's own territory "
          "once the minor/major-piece threshold is met",
          "[eval][space]") {
  BughousePosition pos;
  // Two knights reaching into the White ranks 2-3 zone
  // Plus king's own contribution to that zone
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/1N2K1N1 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  SpaceEvaluator eval;
  // Knights on b1/g1 control {a3,c3,d2} and {e2,f3,h3}
  // King on e1 controls {d2,e2,f2}
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 14);
}

TEST_CASE("SpaceEvaluator credits space on the partner's board using the "
          "partner's colour",
          "[eval][space]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  // Give Black two knights that reach into Black's own ranks-6/7 zone
  pos.boards[1].load_fen("1n2k1n1/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  SpaceEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 14);
}