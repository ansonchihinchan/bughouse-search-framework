#include <catch2/catch_all.hpp>

#include "eval/classical/activity.h"
#include "eval/types.h"
#include "game/bughouse.h"

namespace {
constexpr int UNDEVELOPED_PENALTY = 15;
constexpr int OPEN_FILE_BONUS = 15;
constexpr int SEMI_OPEN_FILE_BONUS = 8;
} // namespace

TEST_CASE("ActivityEvaluator scores bare kings as zero", "[eval][activity]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  ActivityEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("ActivityEvaluator penalizes a minor piece sitting on its home "
          "square",
          "[eval][activity]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  ActivityEvaluator eval;

  BughousePosition undeveloped;
  undeveloped.boards[0].load_fen("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1"); // Nb1
  undeveloped.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition developed;
  developed.boards[0].load_fen("4k3/8/8/8/8/3N4/8/4K3 w - - 0 1"); // Nd3
  developed.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int undeveloped_score =
      eval.evaluate(to_context(undeveloped, search)).mid_game();
  int developed_score = eval.evaluate(to_context(developed, search)).mid_game();

  REQUIRE(undeveloped_score == -UNDEVELOPED_PENALTY);
  REQUIRE(developed_score == 0);
}

TEST_CASE("ActivityEvaluator rewards a rook on a fully open file over a "
          "semi-open or blocked one",
          "[eval][activity]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  ActivityEvaluator eval;

  BughousePosition open_file;
  open_file.boards[0].load_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1"); // Rf1
  open_file.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition semi_open;
  semi_open.boards[0].load_fen("4k3/5p2/8/8/8/8/8/4KR2 w - - 0 1"); // black
                                                                    // pawn f7
  semi_open.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition blocked;
  blocked.boards[0].load_fen("4k3/8/8/8/8/8/5P2/4KR2 w - - 0 1"); // own pawn
                                                                  // f2
  blocked.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int open_score = eval.evaluate(to_context(open_file, search)).mid_game();
  int semi_open_score = eval.evaluate(to_context(semi_open, search)).mid_game();
  int blocked_score = eval.evaluate(to_context(blocked, search)).mid_game();

  REQUIRE(open_score == OPEN_FILE_BONUS);
  REQUIRE(semi_open_score == SEMI_OPEN_FILE_BONUS);
  REQUIRE(blocked_score == 0);
  REQUIRE(open_score > semi_open_score);
  REQUIRE(semi_open_score > blocked_score);
}