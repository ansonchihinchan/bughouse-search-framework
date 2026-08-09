#include <catch2/catch_all.hpp>

#include "communication/message.h"
#include "eval/bughouse/exchange.h"
#include "eval/fixture.h"
#include "eval/types.h"
#include "game/board.h"

TEST_CASE("ExchangeEvaluator scores zero when no pieces are attacked",
          "[eval][bughouse][exchange]") {
  Fixture fx("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  ExchangeEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("ExchangeEvaluator credits an undefended enemy piece we could "
          "capture",
          "[eval][bughouse][exchange]") {
  Fixture fx("4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1");
  ExchangeEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 380);
  REQUIRE(score.end_game() == 380);
}

TEST_CASE("ExchangeEvaluator penalises our own undefended piece the enemy "
          "could capture",
          "[eval][bughouse][exchange]") {
  Fixture fx("3rk3/8/8/3N4/8/8/8/4K3 w - - 0 1");
  ExchangeEvaluator eval;

  EvalScore score = eval.evaluate(fx.context(to_player(0)));
  REQUIRE(score.mid_game() == -380);
  REQUIRE(score.end_game() == -380);
}

TEST_CASE("ExchangeEvaluator penalises hanging material more heavily when "
          "partner is in critical danger",
          "[eval][bughouse][exchange]") {
  Fixture fx("3rk3/8/8/3N4/8/8/8/4K3 w - - 0 1");
  fx.partner.king_danger = 20.f;
  fx.partner.danger = true;
  fx.message.strat_request = {StrategyType::Defend, 1.0f, Urgency::Critical};

  ExchangeEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == -1178);
  REQUIRE(score.end_game() == -912);
}

TEST_CASE("ExchangeEvaluator boosts capture value when partner has "
          "explicitly requested the matching piece type",
          "[eval][bughouse][exchange]") {
  Fixture fx("4k3/8/8/3n4/8/8/8/3RK3 w - - 0 1");
  fx.message.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};

  ExchangeEvaluator eval;
  EvalScore score = eval.evaluate(fx.context(to_player(0)));

  REQUIRE(score.mid_game() == 532);
  REQUIRE(score.end_game() == 608);
}