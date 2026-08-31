#include <catch2/catch_all.hpp>

#include "eval/const.h"
#include "eval/context.h"
#include "game/bughouse.h"
#include "game/piece_value.h"

namespace {
constexpr const char *BARE_KINGS_FEN = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";
constexpr const char *WHITE_KING_UNDER_FIRE_FEN =
    "4k3/8/8/4q3/8/8/8/4K3 w - - 0 1";

BughousePosition bare_position() {
  BughousePosition position;
  position.boards[0].load_fen(BARE_KINGS_FEN);
  position.boards[1].load_fen(BARE_KINGS_FEN);
  return position;
}
} // namespace

TEST_CASE("make_prediction_summary returns a neutral, fully-confident "
          "forecast when the partner sends no signals and faces no king "
          "danger",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  for (int pt = 0; pt < PIECE_TYPE_NO; pt++) {
    REQUIRE(summary.receive_probability[pt] == 0.f);
    REQUIRE(summary.donate_probability[pt] == 0.f);
  }
  REQUIRE(summary.expected_incoming_value == 0.f);
  REQUIRE(summary.expected_outgoing_value == 0.f);
  REQUIRE(summary.attack_confidence == 0.f);
  REQUIRE(summary.defence_confidence == 1.f);
  REQUIRE(summary.volatility == 0.f);
}

TEST_CASE("make_prediction_summary weights a piece request by confidence, "
          "ETA and urgency to predict outgoing material",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};
  message.piece_request = {KNIGHT, 0.8f, Urgency::High, 2};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  float expected_weight = 0.8f * (4.f / 6.f) * 1.0f;
  REQUIRE(summary.donate_probability[KNIGHT] == Catch::Approx(expected_weight));
  REQUIRE(summary.expected_outgoing_value ==
          Catch::Approx(expected_weight * PieceValue::effective_value(KNIGHT)));
}

TEST_CASE("make_prediction_summary clamps the donate weight and outgoing "
          "value contribution to a maximum of one",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};
  message.piece_request = {PAWN, 1.0f, Urgency::Critical, 0};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  REQUIRE(summary.donate_probability[PAWN] == Catch::Approx(1.0f));
  REQUIRE(summary.expected_outgoing_value ==
          Catch::Approx(PieceValue::effective_value(PAWN)));
}

TEST_CASE("make_prediction_summary ignores a piece request whose ETA is "
          "beyond the horizon",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};
  message.piece_request = {QUEEN, 1.0f, Urgency::Critical, 6};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  REQUIRE(summary.donate_probability[QUEEN] == 0.f);
  REQUIRE(summary.expected_outgoing_value == 0.f);
}

TEST_CASE("make_prediction_summary derives attack confidence and expected "
          "incoming value from an AttackNow strategy request",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};
  message.strat_request = {StrategyType::AttackNow, 0.5f, Urgency::Medium};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  float expected_confidence = 0.5f * 0.7f;
  REQUIRE(summary.attack_confidence == Catch::Approx(expected_confidence));
  REQUIRE(summary.receive_probability[KNIGHT] ==
          Catch::Approx(expected_confidence));
  REQUIRE(summary.expected_incoming_value ==
          Catch::Approx(expected_confidence * PieceValue::PIECE_VALUE[KNIGHT]));
}

TEST_CASE("partner messages require the expected sender and expire",
          "[eval][context][communication][freshness]") {
  BughousePosition position = bare_position();
  position.boards[1].fullMove = 10;
  Message message{};
  message.sender = to_player(2);
  message.move_no = 10;
  REQUIRE(is_fresh_partner_message(position, to_player(0), message));

  message.sender = to_player(3);
  REQUIRE_FALSE(is_fresh_partner_message(position, to_player(0), message));
  message.sender = to_player(2);
  message.move_no = 10 - MAX_MESSAGE_AGE - 1;
  REQUIRE_FALSE(is_fresh_partner_message(position, to_player(0), message));
  message.move_no = 11;
  REQUIRE_FALSE(is_fresh_partner_message(position, to_player(0), message));
}

TEST_CASE("make_prediction_summary ignores strategy requests other than "
          "AttackNow when forecasting incoming material",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  Message message{};
  message.strat_request = {StrategyType::Defend, 1.0f, Urgency::Critical};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  REQUIRE(summary.attack_confidence == 0.f);
  REQUIRE(summary.expected_incoming_value == 0.f);
}

TEST_CASE("make_prediction_summary erodes defence confidence and raises "
          "volatility as the partner's king comes under attack",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  position.boards[1].load_fen(WHITE_KING_UNDER_FIRE_FEN);
  Message message{};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  float expected_ratio = 16.f / PARTNER_KING_DANGER_CLAMP;
  REQUIRE(summary.defence_confidence == Catch::Approx(1.f - expected_ratio));
  REQUIRE(summary.volatility == Catch::Approx(expected_ratio));
}

TEST_CASE("make_prediction_summary combines king danger and an urgent piece "
          "request into a higher volatility forecast",
          "[eval][context][prediction]") {
  BughousePosition position = bare_position();
  position.boards[1].load_fen(WHITE_KING_UNDER_FIRE_FEN);
  Message message{};
  message.piece_request = {ROOK, 1.0f, Urgency::Critical, 0};

  PredictionSummary summary =
      make_prediction_summary(position, to_player(0), message);

  float danger_ratio = 16.f / PARTNER_KING_DANGER_CLAMP;
  float expected_volatility = danger_ratio + 1.0f * 1.5f;
  REQUIRE(summary.volatility == Catch::Approx(expected_volatility));
}

TEST_CASE("communication_hash is deterministic for equal contexts",
          "[eval][context][hash]") {
  CommunicationContext a{};
  CommunicationContext b{};
  REQUIRE(communication_hash(a) == communication_hash(b));
}

TEST_CASE("communication_hash changes when the piece request differs",
          "[eval][context][hash]") {
  CommunicationContext a{};
  CommunicationContext b{};
  b.message.piece_request = {KNIGHT, 1.0f, Urgency::Critical, 0};
  REQUIRE(communication_hash(a) != communication_hash(b));
}

TEST_CASE("communication_hash changes when the strategy request differs",
          "[eval][context][hash]") {
  CommunicationContext a{};
  CommunicationContext b{};
  b.message.strat_request = {StrategyType::AttackNow, 0.5f, Urgency::High};
  REQUIRE(communication_hash(a) != communication_hash(b));
}

TEST_CASE("communication_hash changes when the partner's king danger differs",
          "[eval][context][hash]") {
  CommunicationContext a{};
  CommunicationContext b{};
  b.partner.king_danger = 12;
  REQUIRE(communication_hash(a) != communication_hash(b));
}

TEST_CASE("communication_hash changes when the prediction summary differs",
          "[eval][context][hash]") {
  CommunicationContext a{};
  CommunicationContext b{};
  b.prediction.attack_confidence = 0.75f;
  REQUIRE(communication_hash(a) != communication_hash(b));
}

TEST_CASE("communication_hash distinguishes different requested piece types "
          "at otherwise identical field values",
          "[eval][context][hash]") {
  CommunicationContext a{};
  a.message.piece_request = {KNIGHT, 1.0f, Urgency::Low, 0};
  CommunicationContext b{};
  b.message.piece_request = {BISHOP, 1.0f, Urgency::Low, 0};
  REQUIRE(communication_hash(a) != communication_hash(b));
}