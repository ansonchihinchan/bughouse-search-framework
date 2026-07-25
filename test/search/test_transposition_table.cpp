#include <catch2/catch_all.hpp>

#include "search/transposition_table.h"

TEST_CASE("TranspositionTable::probe returns nullptr on an empty table",
          "[search][tt]") {
  TranspositionTable tt(1);
  REQUIRE(tt.probe(12345) == nullptr);
}

TEST_CASE("TranspositionTable::probe returns nullptr for a key that was "
          "never stored",
          "[search][tt]") {
  TranspositionTable tt(1);
  tt.store(1, 4, 10, Move{}, TTBound::EXACT);
  REQUIRE(tt.probe(2) == nullptr);
}

TEST_CASE("store followed by probe with the same key returns the stored "
          "entry verbatim",
          "[search][tt]") {
  TranspositionTable tt(1);
  Move m = Move::normal(12, 28);

  tt.store(555, 4, 250, m, TTBound::EXACT);
  const TTEntry *e = tt.probe(555);

  REQUIRE(e != nullptr);
  REQUIRE(e->key == 555);
  REQUIRE(e->depth == 4);
  REQUIRE(e->score == 250);
  REQUIRE(e->best_move == m);
  REQUIRE(e->bound == TTBound::EXACT);
}

TEST_CASE("store replaces an existing entry when the new depth is at least "
          "as deep",
          "[search][tt]") {
  TranspositionTable tt(1);
  Move m1 = Move::normal(0, 1);
  Move m2 = Move::normal(2, 3);

  tt.store(10, 3, 100, m1, TTBound::EXACT);
  tt.store(10, 6, 200, m2, TTBound::LOWER);

  const TTEntry *e = tt.probe(10);
  REQUIRE(e != nullptr);
  REQUIRE(e->depth == 6);
  REQUIRE(e->score == 200);
  REQUIRE(e->best_move == m2);
  REQUIRE(e->bound == TTBound::LOWER);
}

TEST_CASE("store does not replace an existing entry with a shallower depth "
          "in the same generation",
          "[search][tt]") {
  TranspositionTable tt(1);
  Move m1 = Move::normal(0, 1);
  Move m2 = Move::normal(2, 3);

  tt.store(20, 6, 100, m1, TTBound::EXACT);
  tt.store(20, 3, 999, m2, TTBound::LOWER);

  const TTEntry *e = tt.probe(20);
  REQUIRE(e != nullptr);
  REQUIRE(e->depth == 6);
  REQUIRE(e->score == 100);
  REQUIRE(e->best_move == m1);
}

TEST_CASE("new_search allows a shallower entry to overwrite a stale entry "
          "from a previous generation",
          "[search][tt]") {
  TranspositionTable tt(1);
  Move m1 = Move::normal(0, 1);
  Move m2 = Move::normal(2, 3);

  tt.store(30, 6, 100, m1, TTBound::EXACT);
  tt.new_generation();
  tt.store(30, 1, 555, m2, TTBound::UPPER);

  const TTEntry *e = tt.probe(30);
  REQUIRE(e != nullptr);
  REQUIRE(e->depth == 1);
  REQUIRE(e->score == 555);
  REQUIRE(e->best_move == m2);
  REQUIRE(e->bound == TTBound::UPPER);
}

TEST_CASE("clear removes all entries", "[search][tt]") {
  TranspositionTable tt(1);
  tt.store(40, 5, 10, Move::normal(0, 1), TTBound::EXACT);
  REQUIRE(tt.probe(40) != nullptr);

  tt.clear();

  REQUIRE(tt.probe(40) == nullptr);
}

TEST_CASE("clear resets the generation so a shallow store is accepted again",
          "[search][tt]") {
  TranspositionTable tt(1);
  tt.store(50, 6, 100, Move::normal(0, 1), TTBound::EXACT);
  tt.new_generation();
  tt.clear();

  tt.store(50, 1, 42, Move::normal(2, 3), TTBound::UPPER);

  const TTEntry *e = tt.probe(50);
  REQUIRE(e != nullptr);
  REQUIRE(e->depth == 1);
  REQUIRE(e->score == 42);
}

TEST_CASE("resize discards previous contents and leaves the table usable",
          "[search][tt]") {
  TranspositionTable tt(1);
  tt.store(5, 2, 10, Move{}, TTBound::EXACT);
  REQUIRE(tt.probe(5) != nullptr);

  tt.resize(8);
  REQUIRE(tt.probe(5) == nullptr);

  tt.store(5, 2, 10, Move{}, TTBound::EXACT);
  REQUIRE(tt.probe(5) != nullptr);
}