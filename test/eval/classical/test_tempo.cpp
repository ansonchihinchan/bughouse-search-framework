#include <catch2/catch_all.hpp>

#include "eval/classical/tempo.h"
#include "eval/types.h"
#include "game/bughouse.h"

namespace {
constexpr int TEMPO_BONUS = 12;
} // namespace

TEST_CASE("TempoEvaluator cancels out when White is to move on both boards",
          "[eval][tempo]") {
  BughousePosition pos;
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  TempoEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 0);
}

TEST_CASE("TempoEvaluator rewards the root's own board when it's actually "
          "their move and both boards agree",
          "[eval][tempo]") {
  BughousePosition pos;
  pos.boards[0].load_fen(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  pos.boards[1].load_fen(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  TempoEvaluator eval;
  REQUIRE(eval.evaluate(to_context(pos, search)).mid_game() == 2 * TEMPO_BONUS);
}

TEST_CASE("TempoEvaluator is antisymmetric between opposing players",
          "[eval][tempo]") {
  BughousePosition pos;
  pos.boards[1].load_fen(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
  BughouseClock clock = make_clock();
  TempoEvaluator eval;

  int score0 = eval.evaluate(to_context(pos, make_context(clock, to_player(0))))
                   .mid_game();
  int score1 = eval.evaluate(to_context(pos, make_context(clock, to_player(1))))
                   .mid_game();

  REQUIRE(score0 == -score1);
}