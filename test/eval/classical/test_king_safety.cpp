#include <catch2/catch_all.hpp>

#include "eval/classical/king_safety.h"
#include "eval/types.h"
#include "game/board.h"
#include "search/see.h"

namespace {
constexpr int ATTACK_UNIT_PENALTY = 8;
constexpr int SHIELD_BONUS = 6;
} // namespace

TEST_CASE("KingSafetyEvaluator scores bare kings with no pockets as zero",
          "[eval][king_safety]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  KingSafetyEvaluator eval;
  REQUIRE(eval.evaluate(to_context(board, search)).mid_game() == 0);
}

TEST_CASE("KingSafetyEvaluator rewards a pawn shield in the king's own zone",
          "[eval][king_safety]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  KingSafetyEvaluator eval;

  Board no_shield;
  no_shield.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  Board shielded;
  shielded.load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"); // e2 shield

  int no_shield_score = eval.evaluate(to_context(no_shield, search)).mid_game();
  int shielded_score = eval.evaluate(to_context(shielded, search)).mid_game();

  REQUIRE(no_shield_score == 0);
  REQUIRE(shielded_score == SHIELD_BONUS);
}

TEST_CASE("KingSafetyEvaluator penalizes attackers reaching into the king's "
          "zone",
          "[eval][king_safety]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  KingSafetyEvaluator eval;

  Board board;
  // Black queen on e5 attacks down the e-file into White's king zone
  // Hits e2 and (being blocked by the king) e1 -- 2 squares.
  board.load_fen("4k3/8/8/4q3/8/8/8/4K3 w - - 0 1");

  int score = eval.evaluate(to_context(board, search)).mid_game();
  REQUIRE(score == -(2 * ATTACK_UNIT_PENALTY));
}

TEST_CASE("KingSafetyEvaluator is consistent between opposing players",
          "[eval][king_safety]") {
  Board board;
  board.load_fen("4k3/8/8/4q3/8/8/4P3/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  KingSafetyEvaluator eval;

  int score0 =
      eval.evaluate(to_context(board, make_context(clock, to_player(0))))
          .mid_game();
  int score1 =
      eval.evaluate(to_context(board, make_context(clock, to_player(1))))
          .mid_game();

  REQUIRE(score0 == score1);
}