#include <catch2/catch_all.hpp>

#include "game/types.h"
#include <algorithm>
#include <vector>

TEST_CASE("flip toggles colour", "[types]") {
  REQUIRE(flip(WHITE) == BLACK);
  REQUIRE(flip(BLACK) == WHITE);
}

TEST_CASE("rank_of/file_of/to_square round-trip", "[types]") {
  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      Square sq = to_square(file, rank);
      REQUIRE(rank_of(sq) == rank);
      REQUIRE(file_of(sq) == file);
    }
  }

  // Spot checks against known square indices.
  REQUIRE(to_square(0, 0) == 0);  // a1
  REQUIRE(to_square(7, 7) == 63); // h8
  REQUIRE(to_square(4, 1) == 12); // e2
}

TEST_CASE("square_to_str formats algebraic squares", "[types]") {
  REQUIRE(square_to_str(0) == "a1");
  REQUIRE(square_to_str(63) == "h8");
  REQUIRE(square_to_str(12) == "e2");
  REQUIRE(square_to_str(28) == "e4");
}

TEST_CASE("Piece equality and emptiness", "[types]") {
  Piece empty{};
  REQUIRE(empty.is_empty());

  Piece wp = make_piece(WHITE, PAWN);
  Piece wp2 = make_piece(WHITE, PAWN);
  Piece bp = make_piece(BLACK, PAWN);

  REQUIRE(wp == wp2);
  REQUIRE(wp != bp);
  REQUIRE_FALSE(wp.is_empty());
}

TEST_CASE("Piece::index is unique per (colour, type) and matches layout",
          "[types]") {
  // index() = colour * (PIECE_TYPE_NO - 1) + type - 1
  REQUIRE(make_piece(WHITE, PAWN).index() == 0);
  REQUIRE(make_piece(WHITE, KING).index() == 5);
  REQUIRE(make_piece(BLACK, PAWN).index() == 6);
  REQUIRE(make_piece(BLACK, KING).index() == 11);

  // No two distinct (colour, type) pairs should collide.
  std::vector<int> seen;
  for (Colour c : {WHITE, BLACK}) {
    for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
      int idx = make_piece(c, pt).index();
      REQUIRE(std::find(seen.begin(), seen.end(), idx) == seen.end());
      seen.push_back(idx);
    }
  }
}

TEST_CASE("Piece::to_char maps to standard FEN letters", "[types]") {
  REQUIRE(make_piece(WHITE, PAWN).to_char() == 'P');
  REQUIRE(make_piece(WHITE, KNIGHT).to_char() == 'N');
  REQUIRE(make_piece(WHITE, BISHOP).to_char() == 'B');
  REQUIRE(make_piece(WHITE, ROOK).to_char() == 'R');
  REQUIRE(make_piece(WHITE, QUEEN).to_char() == 'Q');
  REQUIRE(make_piece(WHITE, KING).to_char() == 'K');
  REQUIRE(make_piece(BLACK, PAWN).to_char() == 'p');
  REQUIRE(make_piece(BLACK, KING).to_char() == 'k');
}

TEST_CASE("CastlingRights bitwise composition", "[types]") {
  REQUIRE(static_cast<int>(WHITE_CASTLING) == (WHITE_OO | WHITE_OOO));
  REQUIRE(static_cast<int>(ANY_CASTLING) ==
          (WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO));

  CastlingRights cr = NO_CASTLING;
  cr |= WHITE_OO;
  cr |= BLACK_OOO;
  REQUIRE((cr & WHITE_OO) == WHITE_OO);
  REQUIRE((cr & BLACK_OOO) == BLACK_OOO);
  REQUIRE((cr & WHITE_OOO) == NO_CASTLING);

  cr &= WHITE_OO; // mask down to just WHITE_OO
  REQUIRE(cr == WHITE_OO);
}

TEST_CASE("Move::normal round trips to/from algebraic string", "[types]") {
  Move m = Move::normal(12, 28); // e2e4
  REQUIRE(m.to_string() == "e2e4");
  REQUIRE(m.type == NORMAL);
  REQUIRE_FALSE(m.is_drop());
  REQUIRE_FALSE(m.is_none());
}

TEST_CASE("Move::promote appends lowercase promotion letter", "[types]") {
  Move m = Move::promote(52, 60, QUEEN); // e7e8q
  REQUIRE(m.to_string() == "e7e8q");

  Move mn = Move::promote(52, 60, KNIGHT);
  REQUIRE(mn.to_string() == "e7e8n");
}

TEST_CASE("Move::drop formats as Piece@square and is flagged as a drop",
          "[types]") {
  Move m = Move::drop(KNIGHT, 28); // N@e4
  REQUIRE(m.to_string() == "N@e4");
  REQUIRE(m.is_drop());
  REQUIRE(m.from == -1);
  REQUIRE_FALSE(m.is_none()); // drops are never "none" despite from == -1
}

TEST_CASE("Move::is_none only true for default-constructed non-drop moves",
          "[types]") {
  Move none;
  REQUIRE(none.is_none());

  Move normal = Move::normal(0, 1);
  REQUIRE_FALSE(normal.is_none());

  Move drop = Move::drop(PAWN, 0);
  REQUIRE_FALSE(drop.is_none());
}

TEST_CASE("Move equality compares all fields", "[types]") {
  Move a = Move::promote(12, 4, QUEEN);
  Move b = Move::promote(12, 4, QUEEN);
  Move c = Move::promote(12, 4, KNIGHT);
  Move d = Move::normal(12, 4);

  REQUIRE(a == b);
  REQUIRE(a != c);
  REQUIRE(a != d);
}

TEST_CASE("PlayerId converts to/from int and compares against raw ints",
          "[types][playerid]") {
  REQUIRE(to_int(to_player(0)) == 0);
  REQUIRE(to_int(to_player(3)) == 3);

  REQUIRE(to_player(2) == 2);
  REQUIRE(2 == to_player(2));
  REQUIRE(to_player(1) != 2);

  REQUIRE(to_int(NO_PLAYER) == -1);
}

TEST_CASE("PlayerId::operator^ flips side and partner as expected",
          "[types][playerid]") {
  // Board 0: (0, 1) -- toggling ^1 flips the side to move on a board.
  REQUIRE((to_player(0) ^ 1) == 1);
  REQUIRE((to_player(1) ^ 1) == 0);

  // Board 1: (2, 3)
  REQUIRE((to_player(2) ^ 1) == 3);
  REQUIRE((to_player(3) ^ 1) == 2);

  // Partners: (0, 2), (1, 3) -- toggling ^2 crosses to the partner board.
  REQUIRE((to_player(0) ^ 2) == 2);
  REQUIRE((to_player(1) ^ 2) == 3);
  REQUIRE((to_player(2) ^ 2) == 0);
  REQUIRE((to_player(3) ^ 2) == 1);
}