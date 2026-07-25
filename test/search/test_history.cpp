#include <catch2/catch_all.hpp>

#include "search/history.h"

TEST_CASE("History::score defaults to zero for every piece/square pair",
          "[search][history]") {
  History h;
  Piece p = make_piece(WHITE, PAWN);
  REQUIRE(h.score(p, to_square(4, 3)) == 0);
}

TEST_CASE("History::add increases the score by depth squared",
          "[search][history]") {
  History h;
  Piece p = make_piece(WHITE, KNIGHT);
  Square sq = to_square(2, 4);

  h.add(p, sq, 4); // +16
  REQUIRE(h.score(p, sq) == 16);

  h.add(p, sq, 3); // +9 -> 25
  REQUIRE(h.score(p, sq) == 25);
}

TEST_CASE("History::add clamps to MAX_VALUE", "[search][history]") {
  History h;
  Piece p = make_piece(BLACK, QUEEN);
  Square sq = to_square(0, 0);

  h.add(p, sq, 2000);
  REQUIRE(h.score(p, sq) == History::MAX_VALUE);
}

TEST_CASE("History::add only affects the given piece/square pair",
          "[search][history]") {
  History h;
  Piece rook = make_piece(WHITE, ROOK);
  Piece bishop = make_piece(WHITE, BISHOP);
  Square sq = to_square(3, 3);
  Square other_sq = to_square(3, 4);

  h.add(rook, sq, 4);

  REQUIRE(h.score(rook, sq) > 0);
  REQUIRE(h.score(bishop, sq) == 0);
  REQUIRE(h.score(rook, other_sq) == 0);
}

TEST_CASE("History::age halves every stored score", "[search][history]") {
  History h;
  Piece p = make_piece(WHITE, PAWN);
  Square sq = to_square(4, 1);

  h.add(p, sq, 10); // +100
  REQUIRE(h.score(p, sq) == 100);

  h.age();
  REQUIRE(h.score(p, sq) == 50);

  h.age();
  REQUIRE(h.score(p, sq) == 25);
}

TEST_CASE("History::clear resets every entry to zero", "[search][history]") {
  History h;
  Piece p = make_piece(BLACK, KNIGHT);
  Square sq = to_square(5, 5);

  h.add(p, sq, 5);
  REQUIRE(h.score(p, sq) != 0);

  h.clear();
  REQUIRE(h.score(p, sq) == 0);
}