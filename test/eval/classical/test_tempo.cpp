#include <catch2/catch_all.hpp>

#include "eval/classical/tempo.h"
#include "eval/types.h"
#include "game/board.h"

namespace {
constexpr int TEMPO_BONUS = 12;
} // namespace

TEST_CASE("TempoEvaluator rewards the root's own board", "[eval][tempo]") {
  Board board;
  board.load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  TempoEvaluator eval;
  REQUIRE(eval.evaluate(to_classical_context(board)).mid_game() == TEMPO_BONUS);
}