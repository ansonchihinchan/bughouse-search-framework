#pragma once

#include "communication/channel.h"
#include "communication/message.h"
#include "game/bughouse.h"
#include "game/types.h"
#include <array>
#include <bit>
#include <cstdint>

// TODO
struct PredictionSummary {
  std::array<float, PIECE_TYPE_NO> receive_probability{};
  std::array<float, PIECE_TYPE_NO> donate_probability{};

  float expected_incoming_value = 0.f;
  float expected_outgoing_value = 0.f;

  float attack_confidence = 0.f;
  float defence_confidence = 1.f;

  float volatility = 0.f;
};

struct PartnerContext {
  // board-derived
  int material_balance = 0;
  int king_danger = 0;
  int phase = 0;
};

bool is_dangerous(const Board &board, int phase);

inline constexpr int MAX_MESSAGE_AGE = 2;
bool is_fresh_partner_message(const BughousePosition &position,
                              PlayerId recipient, const Message &message);

struct CommunicationContext {
  // The player this snapshot was frozen for at the start of the search
  // message/prediction are player's team's private communication
  PlayerId origin_player = NO_PLAYER;
  PartnerContext partner;
  std::array<PartnerContext, COLOUR_NO> partner_by_colour{};
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

inline uint64_t hash_combine(uint64_t seed, uint64_t value) {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
  return seed;
}

inline uint64_t hash_float(float value) {
  return static_cast<uint64_t>(std::bit_cast<uint32_t>(value));
}

uint64_t communication_hash(const CommunicationContext &context);