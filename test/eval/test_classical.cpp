#include <catch2/catch_all.hpp>

#include "eval/classical.h"
#include "game/board.h"

TEST_CASE("ClassicalEvaluator gives opposite-coloured callers exactly "
          "opposite scores for the same board",
          "[eval][classical]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4KQ2 w - - 0 1"); // White +Q

  ClassicalEvaluator eval;

  int white_score = eval.evaluate(board, WHITE);
  int black_score = eval.evaluate(board, BLACK);

  REQUIRE(white_score == -black_score);
}

TEST_CASE("ClassicalEvaluator produces a non-zero, materially-sensible score "
          "for a lopsided position",
          "[eval][classical]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1"); // White +R +Q

  ClassicalEvaluator eval;

  int white_score = eval.evaluate(board, WHITE);
  REQUIRE(white_score > 0);

  int black_score = eval.evaluate(board, BLACK);
  REQUIRE(black_score < 0);
}

TEST_CASE("ClassicalEvaluator combines multiple features additively (sanity "
          "check against a hand-picked, materially dominant position)",
          "[eval][classical]") {
  Board board;
  board.load_fen("r3k3/8/8/8/8/8/8/2R1KQ2 w - - 0 1");

  ClassicalEvaluator eval;

  int score = eval.evaluate(board, WHITE);
  REQUIRE(score > 500);
  // comfortably more than a single minor piece's worth
}

TEST_CASE("ClassicalEvaluator::is_noisy is true when the side to move is in "
          "check",
          "[eval][classical][noisy]") {
  Board board;
  // Black king in check
  board.load_fen("4k3/5Q2/5K2/8/8/8/8/8 b - - 0 1");

  ClassicalEvaluator eval; 
  REQUIRE(eval.is_noisy(board));
}

TEST_CASE("ClassicalEvaluator::is_noisy is true when a capture is available",
          "[eval][classical][noisy]") {
  Board board;
  // Qf1 can take e2 pawn
  board.load_fen("4k3/8/8/8/8/8/4p3/4KQ2 w - - 0 1");

  ClassicalEvaluator eval;
  REQUIRE(eval.is_noisy(board));
}

TEST_CASE("ClassicalEvaluator::is_noisy is false in a quiet position with no "
          "captures and no check",
          "[eval][classical][noisy]") {
  Board board;
  ClassicalEvaluator eval;
  REQUIRE_FALSE(eval.is_noisy(board));
}