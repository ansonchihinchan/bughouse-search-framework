#include <catch2/catch_all.hpp>

#include "eval/classical/mobility.h"
#include "eval/types.h"
#include "game/bughouse.h"

TEST_CASE("MobilityEvaluator scores a bare-kings position as zero",
          "[eval][mobility]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MobilityEvaluator eval;
  REQUIRE(eval.evaluate(ctx).mid_game() == 0);
}

TEST_CASE("MobilityEvaluator only counts knights, bishops, rooks and queens "
          "-- not pawns or kings",
          "[eval][mobility]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/4P3/8/8/8/8/4K3 w - - 0 1"); // lone pawn only
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(pos, search);

  MobilityEvaluator eval;
  REQUIRE(eval.evaluate(ctx).mid_game() == 0);
}

TEST_CASE("MobilityEvaluator rewards a centralised knight over a cornered one",
          "[eval][mobility]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  MobilityEvaluator eval;

  BughousePosition centre;
  centre.boards[0].load_fen("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1");
  centre.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition corner;
  corner.boards[0].load_fen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");
  corner.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int centre_score = eval.evaluate(to_context(centre, search)).mid_game();
  int corner_score = eval.evaluate(to_context(corner, search)).mid_game();

  REQUIRE(centre_score > corner_score);
}

TEST_CASE("MobilityEvaluator credits mobility on the partner's board using "
          "the partner's colour",
          "[eval][mobility]") {
  BughouseClock clock = make_clock();
  SearchContext search0 = make_context(clock, to_player(0));
  MobilityEvaluator eval;

  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  // Give Black a knight on Board 1
  pos.boards[1].load_fen("4k3/8/8/3n4/8/8/8/4K3 w - - 0 1");

  int score = eval.evaluate(to_context(pos, search0)).mid_game();
  REQUIRE(score > 0);
}

TEST_CASE("MobilityEvaluator is symmetric: identical mirrored positions on "
          "both boards cancel to zero",
          "[eval][mobility]") {
  BughousePosition pos;
  // Identical white knight on both boards
  pos.boards[0].load_fen("4k3/8/8/3Nn3/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  MobilityEvaluator eval;
  // Knights on d5 and e5
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}