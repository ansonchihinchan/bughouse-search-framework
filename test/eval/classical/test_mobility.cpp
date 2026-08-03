#include <catch2/catch_all.hpp>

#include "eval/classical/mobility.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("MobilityEvaluator scores a bare-kings position as zero",
          "[eval][mobility]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(board, search);

  MobilityEvaluator eval;
  REQUIRE(eval.evaluate(ctx).mid_game() == 0);
}

TEST_CASE("MobilityEvaluator only counts knights, bishops, rooks and queens "
          "-- not pawns or kings",
          "[eval][mobility]") {
  Board board;
  board.load_fen("4k3/8/4P3/8/8/8/8/4K3 w - - 0 1"); // lone pawn only
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  EvalContext ctx = to_context(board, search);

  MobilityEvaluator eval;
  REQUIRE(eval.evaluate(ctx).mid_game() == 0);
}

TEST_CASE("MobilityEvaluator rewards a centralised knight over a cornered one",
          "[eval][mobility]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  MobilityEvaluator eval;

  Board centre;
  centre.load_fen("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1");

  Board corner;
  corner.load_fen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1");

  int centre_score = eval.evaluate(to_context(centre, search)).mid_game();
  int corner_score = eval.evaluate(to_context(corner, search)).mid_game();

  REQUIRE(centre_score > corner_score);
}