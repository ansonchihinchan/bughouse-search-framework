#include <catch2/catch_all.hpp>

#include "communication/message.h"
#include "eval/bughouse/communication.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("CommunicationEvaluator scores zero with no signals present",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  CommunicationEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("CommunicationEvaluator rewards a fulfilled, urgent, imminent "
          "piece request",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[0].add(KNIGHT);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 45);
  REQUIRE(score.end_game() == 67);
}

TEST_CASE("CommunicationEvaluator penalises an unmet piece request",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -27);
  REQUIRE(score.end_game() == -15);
}

TEST_CASE("CommunicationEvaluator ignores a piece request whose ETA is "
          "beyond the horizon",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[0].add(KNIGHT);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 6};

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("CommunicationEvaluator scales by predicted net material flow",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.prediction.expected_incoming = 50.f;
  fx.prediction.expected_outgoing = 20.f;

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 18);
  REQUIRE(score.end_game() == 24);
}

TEST_CASE("CommunicationEvaluator scales an AttackNow request by the "
          "predicted attack confidence",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.partner.strat_request = {StrategyType::AttackNow, 0.8f, Urgency::High};
  fx.prediction.attack_confidence = 0.5f;

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 10);
  REQUIRE(score.end_game() == 6);
}

TEST_CASE("CommunicationEvaluator rewards strategy alignment: "
          "TradeEverything while material-up",
          "[eval][bughouse][communication]") {
  Fixture fx(BARE_KINGS);
  fx.partner.strat_request = {StrategyType::TradeEverything, 0.6f,
                              Urgency::Low};
  fx.partner.material_balance = 50;

  CommunicationEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 7);
  REQUIRE(score.end_game() == 7);
}