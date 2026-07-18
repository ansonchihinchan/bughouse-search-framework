#include <catch2/catch_all.hpp>

#include "eval/classical/pawn.h"
#include "eval/types.h"
#include "game/bughouse.h"

namespace {
constexpr int PASSED_BONUS = 20;
constexpr int ISOLATED_PENALTY = 15;
constexpr int DOUBLED_PENALTY = 10;
} // namespace

TEST_CASE("PawnEvaluator scores an empty board as zero", "[eval][pawn]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  PawnEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("PawnEvaluator rewards an unopposed, isolated passed pawn",
          "[eval][pawn]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"); // lone pawn e2
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  PawnEvaluator eval;
  int score = eval.evaluate(to_context(pos, search)).mid_game();
  REQUIRE(score == PASSED_BONUS - ISOLATED_PENALTY);
}

TEST_CASE("PawnEvaluator removes the passed bonus when an enemy pawn blocks "
          "the file ahead",
          "[eval][pawn]") {
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));
  PawnEvaluator eval;

  BughousePosition unopposed;
  unopposed.boards[0].load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  unopposed.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  BughousePosition blocked;
  blocked.boards[0].load_fen("4k3/8/4p3/8/8/8/4P3/4K3 w - - 0 1");
  blocked.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  int unopposed_score = eval.evaluate(to_context(unopposed, search)).mid_game();
  int blocked_score = eval.evaluate(to_context(blocked, search)).mid_game();

  REQUIRE(unopposed_score == PASSED_BONUS - ISOLATED_PENALTY);
  REQUIRE(blocked_score < unopposed_score);
}

TEST_CASE("PawnEvaluator penalizes doubled pawns on the same file",
          "[eval][pawn]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/4P3/8/4P3/3K4 w - - 0 1"); // e2 + e4
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  PawnEvaluator eval;
  int score = eval.evaluate(to_context(pos, search)).mid_game();
  // Both pawns passed, isolated and doubled
  REQUIRE(score == 2 * (PASSED_BONUS - ISOLATED_PENALTY - DOUBLED_PENALTY));
}

TEST_CASE("PawnEvaluator does not treat pawns on adjacent files as isolated",
          "[eval][pawn]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/3PP3/8/8/4K3 w - - 0 1"); // d4 + e4
  pos.boards[1].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  PawnEvaluator eval;
  int score = eval.evaluate(to_context(pos, search)).mid_game();
  // Both pawns passed, not isolated and not doubled
  REQUIRE(score == 2 * PASSED_BONUS);
}

TEST_CASE("PawnEvaluator is antisymmetric between opposing players",
          "[eval][pawn]") {
  BughousePosition pos;
  pos.boards[0].load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  PawnEvaluator eval;

  int score0 = eval.evaluate(to_context(pos, make_context(clock, to_player(0))))
                   .mid_game();
  int score1 = eval.evaluate(to_context(pos, make_context(clock, to_player(1))))
                   .mid_game();

  REQUIRE(score0 == -score1);
}