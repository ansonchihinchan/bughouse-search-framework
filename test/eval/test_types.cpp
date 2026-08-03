#include <catch2/catch_all.hpp>

#include "eval/types.h"
#include "game/attacks.h"
#include "game/board.h"

TEST_CASE("to_context computes maximum phase for the standard start position",
          "[eval][types][phase]") {
  Board board;
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);
  REQUIRE(ctx.phase == EvalScore::MAX_PHASE);
}

TEST_CASE("to_context computes zero phase for bare kings",
          "[eval][types][phase]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);
  REQUIRE(ctx.phase == 0);
}

TEST_CASE("to_context flags an unopposed pawn as passed and isolated",
          "[eval][types][pawn_info]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/4P3/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);
  Bitboard e2 = 1ULL << to_square(4, 1);

  REQUIRE((ctx.pawn_info.passed[WHITE] & e2) != 0);
  REQUIRE((ctx.pawn_info.isolated[WHITE] & e2) != 0);
  REQUIRE(ctx.pawn_info.doubled[WHITE] == 0);
}

TEST_CASE("to_context does not flag a pawn as passed when blocked by an enemy "
          "pawn ahead on the same file",
          "[eval][types][pawn_info]") {
  Board board;
  board.load_fen("4k3/8/4p3/8/8/8/4P3/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);
  Bitboard e2 = 1ULL << to_square(4, 1);

  REQUIRE((ctx.pawn_info.passed[WHITE] & e2) == 0);
}

TEST_CASE("to_context flags pawns doubled on the same file for both",
          "[eval][types][pawn_info]") {
  Board board;
  board.load_fen("4k3/8/8/8/4P3/8/4P3/3K4 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);
  Bitboard e2 = 1ULL << to_square(4, 1);
  Bitboard e4 = 1ULL << to_square(4, 3);

  REQUIRE((ctx.pawn_info.doubled[WHITE] & e2) != 0);
  REQUIRE((ctx.pawn_info.doubled[WHITE] & e4) != 0);
}

TEST_CASE("to_context computes attack_info.kingZone as king_attacks plus the "
          "king's own square",
          "[eval][types][attack_info]") {
  init_attack_tables();

  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);

  Square e1 = to_square(4, 0);
  Bitboard expected = king_attacks(e1) | (1ULL << e1);
  REQUIRE(ctx.attack_info.kingZone[WHITE] == expected);
}

TEST_CASE("to_context computes non-zero attacks for the side with pieces and "
          "zero for a colour with no pieces",
          "[eval][types][attack_info]") {
  Board board;
  board.load_fen("4k3/8/8/8/8/8/8/1N2K3 w - - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  EvalContext ctx = to_context(board, search);

  REQUIRE(ctx.attack_info.attacks[WHITE] != 0);
  REQUIRE(ctx.attack_info.attacks[BLACK] != 0);
}