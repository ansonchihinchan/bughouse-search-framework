#include <catch2/catch_all.hpp>

#include "eval/classical/tempo.h"
#include "eval/types.h"
#include "game/board.h"

namespace {
constexpr int TEMPO_BONUS = 12;
} // namespace

TEST_CASE("TempoEvaluator rewards the root's own board when it's actually "
          "their move and both boards agree",
          "[eval][tempo]") {
  Board board;
  board.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  BughouseClock clock = make_clock();
  SearchContext search = make_context(clock, to_player(0));

  TempoEvaluator eval;
  REQUIRE(eval.evaluate(to_context(board, search)).mid_game() == TEMPO_BONUS);
}

TEST_CASE("TempoEvaluator is consistnet between opposing players",
          "[eval][tempo]") {
  Board board;
  board.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
  BughouseClock clock = make_clock();
  TempoEvaluator eval;

  int score0 =
      eval.evaluate(to_context(board, make_context(clock, to_player(0))))
          .mid_game();
  int score1 =
      eval.evaluate(to_context(board, make_context(clock, to_player(1))))
          .mid_game();

  REQUIRE(score0 == score1);
}