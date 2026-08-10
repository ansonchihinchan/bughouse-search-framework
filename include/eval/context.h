#pragma once

#include "communication/channel.h"
#include "communication/message.h"
#include "game/bughouse.h"
#include "game/types.h"
#include <array>

// TODO
struct PredictionSummary {
  std::array<float, PIECE_TYPE_NO> receive_probability{};
  std::array<float, PIECE_TYPE_NO> donate_probability{};

  float expected_incoming_value = 0.f;
  float expected_outgoing_value = 0.f;

  float attack_confidence = 0.f;
  float defence_confidence = 0.f;

  float volatility = 0.f;
};

struct PartnerContext {
  // board-derived
  int material_balance = 0;
  int king_danger = 0.f;
  int phase = 0;

  float stall_intent = 0.f;
};

bool is_dangerous(const Board &board, int phase);
bool should_stall(float stall_intent);

struct CommunicationContext {
  PartnerContext partner;
  Message message;
  PredictionSummary prediction;
};

PartnerContext make_partner_context(const BughousePosition &position,
                                    PlayerId partner);

PredictionSummary make_prediction_summary(const BughousePosition &position,
                                          PlayerId root_player,
                                          Message message);

CommunicationContext
make_communication_context(const BughousePosition &position,
                           PlayerId root_player, const Channel &channel);