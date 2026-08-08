#include <catch2/catch_all.hpp>

#include "communication/message.h"
#include "eval/bughouse/partner.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("PartnerEvaluator scores zero with no signals present",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  PartnerEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PartnerEvaluator rewards a fulfilled, urgent, imminent "
          "piece request",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[0].add(KNIGHT);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 45);
  REQUIRE(score.end_game() == 67);
}

TEST_CASE("PartnerEvaluator penalises an unmet piece request",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -27);
  REQUIRE(score.end_game() == -15);
}

TEST_CASE("PartnerEvaluator ignores a piece request whose ETA is "
          "beyond the horizon",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.pockets[0].add(KNIGHT);
  fx.partner.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 6};

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PartnerEvaluator does not scale by predicted net material flow",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.prediction.expected_incoming = 50.f;
  fx.prediction.expected_outgoing = 20.f;

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PartnerEvaluator does not scale an AttackNow request by the "
          "predicted attack confidence",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.partner.strat_request = {StrategyType::AttackNow, 0.8f, Urgency::High};
  fx.prediction.attack_confidence = 0.5f;

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 20);
  REQUIRE(score.end_game() == 12);
}

TEST_CASE("PartnerEvaluator rewards strategy alignment: "
          "TradeEverything while material-up",
          "[eval][bughouse][partner]") {
  Fixture fx(BARE_KINGS);
  fx.partner.strat_request = {StrategyType::TradeEverything, 0.6f,
                              Urgency::Low};
  fx.partner.material_balance = 50;

  PartnerEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 7);
  REQUIRE(score.end_game() == 7);
}