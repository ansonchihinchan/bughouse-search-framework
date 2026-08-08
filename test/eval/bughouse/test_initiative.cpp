#include <catch2/catch_all.hpp>

#include "eval/bughouse/initiative.h"
#include "eval/types.h"
#include "game/bughouse.h"

#include <array>

namespace {
EvalContext make_context(const BughousePosition &position,
                         PlayerId root_player = to_player(0)) {
  return EvalContext{
      to_classical_context(position.boards[board_of(root_player)]),
      BughouseContext{position.pockets, root_player,
                      std::array<int64_t, PLAYER_NO>{}},
      CommunicationContext{}};
}

} // namespace

TEST_CASE("InitiativeEvaluator scores zero for a quiet position with no "
          "checking material and no pockets",
          "[eval][initiative]") {
  BughousePosition position;
  position.boards[0].load_fen("7k/8/8/8/8/8/8/K7 w - - 0 1");

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("InitiativeEvaluator rewards the side to move for an available "
          "checking move",
          "[eval][initiative]") {
  BughousePosition position;
  position.boards[0].load_fen("7k/8/8/6N1/8/8/8/K7 w - - 0 1");

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() > 0);
  REQUIRE(score.end_game() > 0);
}

TEST_CASE("InitiativeEvaluator scores zero when the only extra piece has no "
          "checking reach and no king-zone presence",
          "[eval][initiative]") {
  BughousePosition position;
  position.boards[0].load_fen("7k/8/8/N7/8/8/8/K7 w - - 0 1");

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("InitiativeEvaluator rewards multiple independent attackers "
          "reaching the enemy king zone over a single one",
          "[eval][initiative]") {
  BughousePosition one_attacker;
  one_attacker.boards[0].load_fen("7k/8/8/6N1/8/8/8/K7 w - - 0 1");

  BughousePosition two_attackers;
  two_attackers.boards[0].load_fen("7k/8/5N2/6N1/8/8/8/K7 w - - 0 1");

  InitiativeEvaluator eval;
  EvalScore one_score = eval.evaluate(make_context(one_attacker));
  EvalScore two_score = eval.evaluate(make_context(two_attackers));

  REQUIRE(two_score.mid_game() > one_score.mid_game());
  REQUIRE(two_score.end_game() > one_score.end_game());
}

TEST_CASE("InitiativeEvaluator credits reserve checking material even off "
          "the mover's turn",
          "[eval][initiative]") {
  constexpr int DROP_CHECK_READY_MID = 14;
  constexpr int DROP_CHECK_READY_END = 22;

  BughousePosition position;
  position.boards[0].load_fen("7k/8/8/8/8/8/8/K7 w - - 0 1");
  position.pockets[0].add(QUEEN);

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() == DROP_CHECK_READY_MID);
  REQUIRE(score.end_game() == DROP_CHECK_READY_END);
}

TEST_CASE("InitiativeEvaluator rewards attacking tempo when the opponent is "
          "the one forced to respond to check",
          "[eval][initiative]") {
  BughousePosition position;
  position.boards[0].load_fen("4k3/8/8/8/8/8/8/K3R3 b - - 0 1");

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() > 0);
  REQUIRE(score.end_game() > 0);
}

TEST_CASE("InitiativeEvaluator penalises attacking tempo when the root "
          "player is the one forced to respond to check",
          "[eval][initiative]") {
  BughousePosition position;
  position.boards[0].load_fen("k3r3/8/8/8/8/8/8/4K3 w - - 0 1");

  InitiativeEvaluator eval;
  EvalScore score = eval.evaluate(make_context(position));

  REQUIRE(score.mid_game() < 0);
  REQUIRE(score.end_game() < 0);
}