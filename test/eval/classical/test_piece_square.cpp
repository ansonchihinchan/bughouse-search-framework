#include <catch2/catch_all.hpp>

#include "eval/classical/piece_square.h"
#include "eval/classical/pst.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("PieceSquareEvaluator scores the standard start position as zero",
          "[eval][piece_square]") {
  Board board;
  ClassicalContext ctx = to_classical_context(board);

  PieceSquareEvaluator eval;
  EvalScore score = eval.evaluate(ctx);
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PieceSquareEvaluator rewards a knight on a strong central square "
          "over the same knight on its back-rank corner",
          "[eval][piece_square]") {
  PieceSquareEvaluator eval;

  Board centre;
  centre.load_fen("4k3/8/8/3N4/8/8/8/4K3 w - - 0 1"); // Nd5

  Board corner;
  corner.load_fen("4k3/8/8/8/8/8/8/N3K3 w - - 0 1"); // Na1

  int centre_score = eval.evaluate(to_classical_context(centre)).mid_game();
  int corner_score = eval.evaluate(to_classical_context(corner)).mid_game();

  REQUIRE(centre_score > corner_score);
}

TEST_CASE("PieceSquareEvaluator mirrors PAWN_PST vertically for Black",
          "[eval][piece_square]") {
  PieceSquareEvaluator eval;

  // white pawn and black pawn one square away from promotion
  Board white_advanced;
  white_advanced.load_fen("4k3/4P3/8/8/8/8/8/4K3 w - - 0 1");

  Board black_advanced;
  black_advanced.load_fen("4k3/8/8/8/8/8/4p3/4K3 w - - 0 1");

  int white_score =
      eval.evaluate(to_classical_context(white_advanced)).mid_game();
  int black_score =
      eval.evaluate(to_classical_context(black_advanced)).mid_game();

  REQUIRE(white_score == -black_score);
  REQUIRE(white_score > 0);
}

TEST_CASE("PieceSquareEvaluator uses phase-dependent king tables (mid vs "
          "endgame placement differ)",
          "[eval][piece_square]") {
  PieceSquareEvaluator eval;

  // King in the centre
  Board centred_king;
  centred_king.load_fen("8/8/8/3K4/8/8/8/7k w - - 0 1");

  EvalScore score = eval.evaluate(to_classical_context(centred_king));

  REQUIRE(score.mid_game() < score.end_game());
}