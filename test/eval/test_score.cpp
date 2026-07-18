#include <catch2/catch_all.hpp>

#include "eval/score.h"

TEST_CASE("EvalScore single-value constructor sets both phases equal",
          "[eval][score]") {
  EvalScore s(42);
  REQUIRE(s.mid_game() == 42);
  REQUIRE(s.end_game() == 42);
}

TEST_CASE("EvalScore two-value constructor stores mid/end independently",
          "[eval][score]") {
  EvalScore s(10, -5);
  REQUIRE(s.mid_game() == 10);
  REQUIRE(s.end_game() == -5);
}

TEST_CASE("EvalScore::operator+= adds both phases component-wise",
          "[eval][score]") {
  EvalScore a(10, 20);
  EvalScore b(1, 2);
  a += b;
  REQUIRE(a.mid_game() == 11);
  REQUIRE(a.end_game() == 22);
}

TEST_CASE("EvalScore::operator-= subtracts both phases component-wise",
          "[eval][score]") {
  EvalScore a(10, 20);
  EvalScore b(1, 2);
  a -= b;
  REQUIRE(a.mid_game() == 9);
  REQUIRE(a.end_game() == 18);
}

TEST_CASE("EvalScore::operator+ and operator- leave operands unmodified",
          "[eval][score]") {
  EvalScore a(10, 20);
  EvalScore b(1, 2);

  EvalScore sum = a + b;
  EvalScore diff = a - b;

  REQUIRE(a.mid_game() == 10);
  REQUIRE(a.end_game() == 20);

  REQUIRE(sum.mid_game() == 11);
  REQUIRE(sum.end_game() == 22);
  REQUIRE(diff.mid_game() == 9);
  REQUIRE(diff.end_game() == 18);
}

TEST_CASE("EvalScore::final at maximum phase returns the mid-game value",
          "[eval][score]") {
  EvalScore s(100, 0);
  REQUIRE(s.final(EvalScore::MAX_PHASE) == 100);
}

TEST_CASE("EvalScore::final at zero phase returns the end-game value",
          "[eval][score]") {
  EvalScore s(100, 0);
  REQUIRE(s.final(0) == 0);
}

TEST_CASE("EvalScore::final interpolates linearly between phases",
          "[eval][score]") {
  // 240 divides evenly by MAX_PHASE (24) so the arithmetic is exact.
  EvalScore s(240, 0);
  REQUIRE(s.final(24) == 240);
  REQUIRE(s.final(12) == 120);
  REQUIRE(s.final(6) == 60);
  REQUIRE(s.final(0) == 0);
}

TEST_CASE("EvalScore::final is phase-independent when mid == end",
          "[eval][score]") {
  EvalScore s(77);
  REQUIRE(s.final(0) == 77);
  REQUIRE(s.final(12) == 77);
  REQUIRE(s.final(24) == 77);
}

TEST_CASE("EvalScore::final blends negative and positive components",
          "[eval][score]") {
  EvalScore s(-100, 100);
  REQUIRE(s.final(24) == -100);
  REQUIRE(s.final(0) == 100);
  REQUIRE(s.final(12) == 0);
}