#include <catch2/catch_all.hpp>

#include "eval/classical/pawn.h"
#include "eval/types.h"
#include "game/board.h"

namespace {
constexpr int PASSED_BONUS = 20;
constexpr int ISOLATED_PENALTY = 15;
constexpr int DOUBLED_PENALTY = 10;
} // namespace

TEST_CASE("PawnEvaluator scores an empty board as zero", "[eval][pawn]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

  PawnEvaluator eval;
  REQUIRE(eval.evaluate(to_classical_context(board)).mid_game() == 0);
}

TEST_CASE("PawnEvaluator rewards an unopposed, isolated passed pawn",
          "[eval][pawn]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1"); // lone pawn e2

  PawnEvaluator eval;
  int score = eval.evaluate(to_classical_context(board)).mid_game();
  REQUIRE(score == PASSED_BONUS - ISOLATED_PENALTY);
}

TEST_CASE("PawnEvaluator removes the passed bonus when an enemy pawn blocks "
          "the file ahead",
          "[eval][pawn]") {
  PawnEvaluator eval;

  Board unopposed;
  unopposed.load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");

  Board blocked;
  blocked.load_fen("4k3/8/4p3/8/8/8/4P3/4K3 w - - 0 1");

  int unopposed_score =
      eval.evaluate(to_classical_context(unopposed)).mid_game();
  int blocked_score = eval.evaluate(to_classical_context(blocked)).mid_game();

  REQUIRE(unopposed_score == PASSED_BONUS - ISOLATED_PENALTY);
  REQUIRE(blocked_score < unopposed_score);
}

TEST_CASE("PawnEvaluator penalises doubled pawns on the same file",
          "[eval][pawn]") {
  Board board;
  board.load_fen("4k3/8/8/8/4P3/8/4P3/3K4 w - - 0 1"); // e2 + e4

  PawnEvaluator eval;
  int score = eval.evaluate(to_classical_context(board)).mid_game();
  // Both pawns passed, isolated and doubled
  REQUIRE(score == 2 * (PASSED_BONUS - ISOLATED_PENALTY - DOUBLED_PENALTY));
}

TEST_CASE("PawnEvaluator does not treat pawns on adjacent files as isolated",
          "[eval][pawn]") {
  Board board;
  board.load_fen("4k3/8/8/8/3PP3/8/8/4K3 w - - 0 1"); // d4 + e4

  PawnEvaluator eval;
  int score = eval.evaluate(to_classical_context(board)).mid_game();
  // Both pawns passed, not isolated and not doubled
  REQUIRE(score == 2 * PASSED_BONUS);
}