#include <catch2/catch_all.hpp>

#include "game/movegen.h"
#include "game/pocket.h"
#include <algorithm>

namespace {
bool contains_move(const std::vector<Move> &moves, Move m) {
  return std::find(moves.begin(), moves.end(), m) != moves.end();
}

int count_type(const std::vector<Move> &moves, MoveType type) {
  return static_cast<int>(
      std::count_if(moves.begin(), moves.end(),
                    [type](const Move &m) { return m.type == type; }));
}
} // namespace

TEST_CASE("generate_moves produces the standard 20 opening moves",
          "[movegen]") {
  Board b;
  auto moves = generate_moves(b);
  REQUIRE(moves.size() == 20);
}

TEST_CASE("generate_moves includes single and double pawn pushes",
          "[movegen][pawn]") {
  Board b;
  auto moves = generate_moves(b);
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 3))));
}

TEST_CASE("generate_moves excludes double push when blocked",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/4n3/8/4P3/7K w - - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 3))));
  // Single push onto e3 is also blocked path-wise? e3 empty here so allowed.
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE("generate_moves excludes single push when the target is occupied",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/8/4n3/4P3/7K w - - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE("generate_moves gives pawns diagonal captures only onto enemy pieces",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/8/3p1p2/4P3/7K w - - 0 1");
  auto moves = generate_moves(b);
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(3, 2))));
  REQUIRE(contains_move(moves, Move::normal(to_square(4, 1), to_square(5, 2))));
}

TEST_CASE("generate_moves does not let pawns capture straight ahead",
          "[movegen][pawn]") {
  Board b("k7/8/8/8/8/4p3/4P3/7K w - - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::normal(to_square(4, 1), to_square(4, 2))));
}

TEST_CASE("generate_moves produces all four promotion piece choices",
          "[movegen][promotion]") {
  Board b("7k/P7/8/8/8/8/8/7K w - - 0 1");
  auto moves = generate_moves(b);
  Square from = to_square(0, 6);
  Square to = to_square(0, 7);
  REQUIRE(contains_move(moves, Move::promote(from, to, QUEEN)));
  REQUIRE(contains_move(moves, Move::promote(from, to, ROOK)));
  REQUIRE(contains_move(moves, Move::promote(from, to, BISHOP)));
  REQUIRE(contains_move(moves, Move::promote(from, to, KNIGHT)));
}

TEST_CASE("generate_moves includes en passant capture immediately after a "
          "double push",
          "[movegen][enpassant]") {
  Board b("7k/8/8/3pP3/8/8/8/7K w - d6 0 2");
  auto moves = generate_moves(b);
  REQUIRE(
      contains_move(moves, Move::en_passant(to_square(4, 4), to_square(3, 5))));
}

TEST_CASE("knight move generation ignores blocking pieces",
          "[movegen][knight]") {
  // Knight surrounded by friendly pieces still jumps over them.
  Board b("k7/8/8/2PPP3/2PNP3/2PPP3/8/7K w - - 0 1");
  auto moves = generate_moves(b);
  int from = to_square(3, 3);
  REQUIRE(contains_move(moves, Move::normal(from, to_square(4, 5))));
  REQUIRE(contains_move(moves, Move::normal(from, to_square(2, 5))));
}

TEST_CASE("sliding pieces stop at the first blocker and cannot jump over it",
          "[movegen][sliding]") {
  Board b("k7/8/8/8/3p4/8/3R4/7K w - - 0 1");
  auto moves = generate_moves(b);
  int from = to_square(3, 1);
  // Rook can capture the pawn on d4...
  REQUIRE(contains_move(moves, Move::normal(from, to_square(3, 3))));
  // ...but cannot continue past it to d5.
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 4))));
}

TEST_CASE("sliding pieces cannot capture a friendly piece",
          "[movegen][sliding]") {
  Board b("k7/8/8/8/3P4/8/3R4/7K w - - 0 1");
  auto moves = generate_moves(b);
  int from = to_square(3, 1);
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 3))));
  REQUIRE_FALSE(contains_move(moves, Move::normal(from, to_square(3, 4))));
}

TEST_CASE("generate_moves includes kingside and queenside castling when legal",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  auto moves = generate_moves(b);
  REQUIRE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
  REQUIRE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
}

TEST_CASE("generate_moves excludes castling through an attacked square",
          "[movegen][castling]") {
  // Black rook on f8 attacks f1, the square the king passes through.
  Board b("r3k2r/8/8/8/8/8/5r2/R3K2R w KQkq - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
}

TEST_CASE("generate_moves excludes castling while in check",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/4r3/R3K2R w KQkq - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
}

TEST_CASE("generate_moves excludes castling when squares between king and "
          "rook are occupied",
          "[movegen][castling]") {
  Board b("r3k2r/8/8/8/8/8/8/RN2K1NR w KQkq - 0 1");
  auto moves = generate_moves(b);
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(2, 0))));
  REQUIRE_FALSE(
      contains_move(moves, Move::castling(to_square(4, 0), to_square(6, 0))));
}

TEST_CASE("generate_drops produces no drops for an empty pocket",
          "[movegen][drops]") {
  Board b;
  Pocket p;
  auto drops = generate_drops(b, p);
  REQUIRE(drops.empty());
}

TEST_CASE("generate_drops targets every empty square for a held piece type",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(KNIGHT);
  auto drops = generate_drops(b, p);
  // 64 squares minus the 2 occupied by kings.
  REQUIRE(drops.size() == 62);
}

TEST_CASE("generate_drops excludes rank 1 and rank 8 for pawn drops",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(PAWN);
  auto drops = generate_drops(b, p);
  for (auto &m : drops) {
    REQUIRE(rank_of(m.to) != 0);
    REQUIRE(rank_of(m.to) != 7);
  }
  // 64 squares - 16 (two back ranks)
  REQUIRE(drops.size() == 48);
}

TEST_CASE("generate_moves with a pocket appends legal drop moves",
          "[movegen][drops]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Pocket p;
  p.add(QUEEN);
  auto moves = generate_moves(b, &p);
  REQUIRE(contains_move(moves, Move::drop(QUEEN, to_square(4, 3))));
}

TEST_CASE("perft(1) from the start position equals 20", "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 1) == 20);
}

TEST_CASE("perft(2) from the start position equals 400", "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 2) == 400);
}

TEST_CASE("perft(0) always returns 1 regardless of position",
          "[movegen][perft]") {
  Board b;
  REQUIRE(perft(b, 0) == 1);
}