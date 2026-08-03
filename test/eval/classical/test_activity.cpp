#include <catch2/catch_all.hpp>

#include "eval/classical/activity.h"
#include "eval/types.h"
#include "game/board.h"

namespace {
constexpr int UNDEVELOPED_PENALTY = 15;
constexpr int OPEN_FILE_BONUS = 15;
constexpr int SEMI_OPEN_FILE_BONUS = 8;
} // namespace

TEST_CASE("ActivityEvaluator scores bare kings as zero", "[eval][activity]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  ActivityEvaluator eval;
  REQUIRE(eval.evaluate(to_context(board, search)).mid_game() == 0);
}

TEST_CASE("ActivityEvaluator penalizes a minor piece sitting on its home "
          "square",
          "[eval][activity]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  ActivityEvaluator eval;

  Board undeveloped;
  undeveloped.load_fen("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1"); // Nb1

  Board developed;
  developed.load_fen("4k3/8/8/8/8/3N4/8/4K3 w - - 0 1"); // Nd3

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

  Board open_file;
  open_file.load_fen("4k3/8/8/8/8/8/8/4KR2 w - - 0 1"); // Rf1

  Board semi_open;
  semi_open.load_fen("4k3/5p2/8/8/8/8/8/4KR2 w - - 0 1"); // black pawn f7

  Board blocked;
  blocked.load_fen("4k3/8/8/8/8/8/5P2/4KR2 w - - 0 1"); // white pawn f2

  int open_score = eval.evaluate(to_context(open_file, search)).mid_game();
  int semi_open_score = eval.evaluate(to_context(semi_open, search)).mid_game();
  int blocked_score = eval.evaluate(to_context(blocked, search)).mid_game();

  REQUIRE(open_score == OPEN_FILE_BONUS);
  REQUIRE(semi_open_score == SEMI_OPEN_FILE_BONUS);
  REQUIRE(blocked_score == 0);
  REQUIRE(open_score > semi_open_score);
  REQUIRE(semi_open_score > blocked_score);
}