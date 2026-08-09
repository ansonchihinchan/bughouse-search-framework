#include <catch2/catch_all.hpp>

#include "eval/bughouse/prediction.h"
#include "eval/types.h"
#include "game/bughouse.h"

#include <array>

namespace {
EvalContext make_context(const PredictionSummary &prediction) {
  BughousePosition position;

  CommunicationContext comm;
  comm.prediction = prediction;

  return EvalContext{make_classical_context(position.boards[0]),
                     BughouseContext{position.pockets, to_player(0),
                                     std::array<int64_t, PLAYER_NO>{}},
                     comm};
}

} // namespace

TEST_CASE("PredictionEvaluator scores zero for a neutral forecast with full "
          "defence confidence",
          "[eval][prediction]") {
  PredictionSummary prediction;
  prediction.defence_confidence = 1.f;

  PredictionEvaluator eval;
  EvalScore score = eval.evaluate(make_context(prediction));

  REQUIRE(score.mid_game() == 0);
  REQUIRE(score.end_game() == 0);
}

TEST_CASE("PredictionEvaluator penalises low defence confidence even with "
          "no other active signal",
          "[eval][prediction]") {
  PredictionSummary prediction;

  PredictionEvaluator eval;
  EvalScore score = eval.evaluate(make_context(prediction));

  REQUIRE(score.mid_game() == -20);
  REQUIRE(score.end_game() == -32);
}

TEST_CASE("PredictionEvaluator weights predicted net material flow more "
          "heavily toward the endgame",
          "[eval][prediction]") {
  PredictionSummary prediction;
  prediction.defence_confidence = 1.f;
  prediction.expected_incoming_value = 200.f;
  prediction.expected_outgoing_value = 0.f;

  PredictionEvaluator eval;
  EvalScore score = eval.evaluate(make_context(prediction));

  REQUIRE(score.mid_game() == 120);
  REQUIRE(score.end_game() == 160);
  REQUIRE(score.end_game() > score.mid_game());
}

TEST_CASE("PredictionEvaluator weights anticipated pieces by both "
          "probability and material value",
          "[eval][prediction]") {
  PredictionSummary queen_incoming;
  queen_incoming.defence_confidence = 1.f;
  queen_incoming.receive_probability[QUEEN] = 1.f;

  PredictionSummary knight_incoming;
  knight_incoming.defence_confidence = 1.f;
  knight_incoming.receive_probability[KNIGHT] = 1.f;

  PredictionEvaluator eval;
  EvalScore queen_score = eval.evaluate(make_context(queen_incoming));
  EvalScore knight_score = eval.evaluate(make_context(knight_incoming));

  REQUIRE(queen_score.mid_game() == 135);
  REQUIRE(queen_score.end_game() == 225);
  REQUIRE(knight_score.mid_game() == 48);
  REQUIRE(knight_score.end_game() == 80);

  REQUIRE(queen_score.mid_game() > knight_score.mid_game());
  REQUIRE(queen_score.end_game() > knight_score.end_game());
}

TEST_CASE("PredictionEvaluator rewards attack confidence independently of "
          "defence risk",
          "[eval][prediction]") {
  PredictionSummary prediction;
  prediction.attack_confidence = 1.f;
  prediction.defence_confidence = 1.f;

  PredictionEvaluator eval;
  EvalScore score = eval.evaluate(make_context(prediction));

  REQUIRE(score.mid_game() == 30);
  REQUIRE(score.end_game() == 20);
}

TEST_CASE("PredictionEvaluator dampens predicted terms as forecast "
          "volatility rises",
          "[eval][prediction]") {
  PredictionSummary stable;
  stable.attack_confidence = 1.f;
  stable.defence_confidence = 1.f;
  stable.volatility = 0.f;

  PredictionSummary noisy = stable;
  noisy.volatility = 2.f;

  PredictionEvaluator eval;
  EvalScore stable_score = eval.evaluate(make_context(stable));
  EvalScore noisy_score = eval.evaluate(make_context(noisy));

  REQUIRE(stable_score.mid_game() == 30);
  REQUIRE(noisy_score.mid_game() == 15);
  REQUIRE(noisy_score.mid_game() < stable_score.mid_game());
  REQUIRE(noisy_score.end_game() < stable_score.end_game());
}