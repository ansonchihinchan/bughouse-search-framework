#include <catch2/catch_all.hpp>

#include "search/see.h"

namespace {
int effective_value(PieceType pt) {
  return SEE::PIECE_VALUE[pt] + SEE::POCKET_BONUS[pt];
}
} // namespace

TEST_CASE("SEE::see_result returns the default (zero) result for a "
          "non-capturing move",
          "[search][see]") {
  Board b;
  Move e4 = Move::normal(to_square(4, 1), to_square(4, 3)); // e2e4

  SEE::Result result = SEE::see_result(b, e4);

  REQUIRE(result.score == 0);
  REQUIRE_FALSE(result.undefended);
  REQUIRE_FALSE(result.king_exposed);
}

TEST_CASE("SEE::see_result returns the default result for drop moves",
          "[search][see]") {
  Board b("k7/8/8/8/8/8/8/7K w - - 0 1");
  Move drop = Move::drop(QUEEN, to_square(4, 3));

  SEE::Result result = SEE::see_result(b, drop);

  REQUIRE(result.score == 0);
  REQUIRE_FALSE(result.undefended);
}

TEST_CASE("SEE credits an undefended capture with its full effective_value",
          "[search][see]") {
  // Position where White (player 0) has Rd1xd2 winning the black queen for free
  Board b("4k3/8/8/8/8/8/3q4/3RK3 w - - 0 1");
  Move rxd2 = Move::normal(to_square(3, 0), to_square(3, 1));

  SEE::Result result = SEE::see_result(b, rxd2);

  REQUIRE(result.score == effective_value(QUEEN));
  REQUIRE(result.undefended);
  REQUIRE_FALSE(result.king_exposed);
}

TEST_CASE("SEE::see_score matches see_result's score", "[search][see]") {
  Board b("4k3/8/8/8/8/8/3q4/3RK3 w - - 0 1");
  Move rxd2 = Move::normal(to_square(3, 0), to_square(3, 1));

  REQUIRE(SEE::see_score(b, rxd2) == SEE::see_result(b, rxd2).score);
}

TEST_CASE("SEE evaluates an even pawn-for-pawn trade as zero",
          "[search][see]") {
  Board b("4k3/8/8/2p5/3p4/4P3/8/4K3 w - - 0 1");
  Move exd4 = Move::normal(to_square(4, 2), to_square(3, 3));

  SEE::Result result = SEE::see_result(b, exd4);

  REQUIRE(result.score == 0);
  REQUIRE_FALSE(result.undefended);
}

TEST_CASE("SEE strongly penalizes capturing a pawn defended by a pawn with a "
          "queen",
          "[search][see]") {
  Board b("4k3/8/8/2p5/3p4/4Q3/8/4K3 w - - 0 1");
  Move qxd4 = Move::normal(to_square(4, 2), to_square(3, 3));

  SEE::Result result = SEE::see_result(b, qxd4);

  REQUIRE(result.score == effective_value(PAWN) - effective_value(QUEEN));
  REQUIRE(result.score < 0);
  REQUIRE_FALSE(result.undefended);
}

TEST_CASE("SEE handles en passant captures using the passed pawn's value",
          "[search][see]") {
  Board b("7k/8/8/3pP3/8/8/8/7K w - d6 0 2");
  Move ep = Move::en_passant(to_square(4, 4), to_square(3, 5)); // e5xd6

  SEE::Result result = SEE::see_result(b, ep);

  REQUIRE(result.score == effective_value(PAWN));
  REQUIRE(result.undefended);
}

TEST_CASE("SEE adds a promotion bonus on top of the captured piece's value",
          "[search][see]") {
  // b7 captures undefended black rook on a8 and promotes
  Board b("r6k/1P6/8/8/8/8/8/7K w - - 0 1");
  Move promo_capture =
      Move::promote(to_square(1, 6), to_square(0, 7), QUEEN); // b7xa8=Q

  SEE::Result result = SEE::see_result(b, promo_capture);

  int expected = effective_value(ROOK) +
                 (SEE::PIECE_VALUE[QUEEN] - SEE::PIECE_VALUE[PAWN]);
  REQUIRE(result.score == expected);
  REQUIRE(result.undefended);
  REQUIRE_FALSE(result.king_exposed);
}

TEST_CASE("SEE penalizes captures that expose the mover's own king",
          "[search][see]") {
  // White rook on e3 is pinned to the king on e1 by the black rook on e7
  Board b("4k3/4r3/8/8/8/3pR3/8/4K3 w - - 0 1");
  Move rxd3 = Move::normal(to_square(4, 2), to_square(3, 2));

  SEE::Result result = SEE::see_result(b, rxd3);

  REQUIRE(result.king_exposed);
  REQUIRE(result.score == effective_value(PAWN) - SEE::PIECE_VALUE[QUEEN] / 6);
}