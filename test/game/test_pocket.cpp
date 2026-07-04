#include <catch2/catch_all.hpp>

#include "game/pocket.h"
#include <iostream>
#include <sstream>

TEST_CASE("Pocket starts empty", "[pocket]") {
  Pocket p;
  REQUIRE(p.empty());
  for (int pt = PAWN; pt <= QUEEN; pt++)
    REQUIRE(p.count(static_cast<PieceType>(pt)) == 0);
}

TEST_CASE("Pocket::add increases count and contains", "[pocket]") {
  Pocket p;
  p.add(PAWN);
  p.add(PAWN);
  p.add(KNIGHT);

  REQUIRE(p.count(PAWN) == 2);
  REQUIRE(p.count(KNIGHT) == 1);
  REQUIRE(p.contains(PAWN));
  REQUIRE(p.contains(KNIGHT));
  REQUIRE_FALSE(p.contains(BISHOP));
  REQUIRE_FALSE(p.empty());
}

TEST_CASE("Pocket::add refuses KING and NO_PIECE_TYPE", "[pocket]") {
  Pocket p;
  p.add(KING);
  p.add(NO_PIECE_TYPE);

  REQUIRE(p.count(KING) == 0);
  REQUIRE(p.count(NO_PIECE_TYPE) == 0);
  REQUIRE(p.empty());
}

TEST_CASE("Pocket::remove decreases count and never goes negative",
          "[pocket]") {
  Pocket p;
  p.add(ROOK);
  REQUIRE(p.count(ROOK) == 1);

  p.remove(ROOK);
  REQUIRE(p.count(ROOK) == 0);

  // Removing from an already-empty slot must not underflow.
  p.remove(ROOK);
  REQUIRE(p.count(ROOK) == 0);
}

TEST_CASE("Pocket::empty reflects all piece types, not just one", "[pocket]") {
  Pocket p;
  p.add(QUEEN);
  REQUIRE_FALSE(p.empty());
  p.remove(QUEEN);
  REQUIRE(p.empty());
}

TEST_CASE("Pocket::print reports (empty) for a fresh pocket", "[pocket]") {
  Pocket p;
  std::ostringstream captured;
  std::streambuf *old = std::cout.rdbuf(captured.rdbuf());
  p.print();
  std::cout.rdbuf(old);

  REQUIRE(captured.str() == "(empty)\n");
}

TEST_CASE("Pocket::print lists one letter per held piece", "[pocket]") {
  Pocket p;
  p.add(PAWN);
  p.add(PAWN);
  p.add(KNIGHT);

  std::ostringstream captured;
  std::streambuf *old = std::cout.rdbuf(captured.rdbuf());
  p.print();
  std::cout.rdbuf(old);

  // Order follows PAWN..QUEEN, then repetitions within a type.
  REQUIRE(captured.str() == "PPN\n");
}