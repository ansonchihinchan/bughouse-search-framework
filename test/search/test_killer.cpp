#include <catch2/catch_all.hpp>

#include "search/killer.h"

TEST_CASE("Killer starts with no moves recorded for any ply",
          "[search][killer]") {
  Killer k;
  REQUIRE(k.first(0).is_none());
  REQUIRE(k.second(0).is_none());
  REQUIRE(k.first(Killer::MAX_PLY - 1).is_none());
}

TEST_CASE("Killer::update stores the first killer for a ply",
          "[search][killer]") {
  Killer k;
  Move m1 = Move::normal(0, 1);

  k.update(5, m1);

  REQUIRE(k.first(5) == m1);
  REQUIRE(k.second(5).is_none());
}

TEST_CASE("Killer::update shifts the previous first killer into second",
          "[search][killer]") {
  Killer k;
  Move m1 = Move::normal(0, 1);
  Move m2 = Move::normal(2, 3);

  k.update(5, m1);
  k.update(5, m2);

  REQUIRE(k.first(5) == m2);
  REQUIRE(k.second(5) == m1);
}

TEST_CASE("Killer::update does not duplicate a move already stored as the "
          "first killer",
          "[search][killer]") {
  Killer k;
  Move m1 = Move::normal(0, 1);

  k.update(3, m1);
  k.update(3, m1);

  REQUIRE(k.first(3) == m1);
  REQUIRE(k.second(3).is_none());
}

TEST_CASE("Killer moves are tracked independently per ply",
          "[search][killer]") {
  Killer k;
  Move m1 = Move::normal(0, 1);
  Move m2 = Move::normal(4, 5);

  k.update(1, m1);
  k.update(2, m2);

  REQUIRE(k.first(1) == m1);
  REQUIRE(k.first(2) == m2);
  REQUIRE(k.second(1).is_none());
  REQUIRE(k.second(2).is_none());
}

TEST_CASE("Killer::clear wipes every ply", "[search][killer]") {
  Killer k;
  k.update(0, Move::normal(0, 1));
  k.update(1, Move::normal(2, 3));

  k.clear();

  REQUIRE(k.first(0).is_none());
  REQUIRE(k.first(1).is_none());
}