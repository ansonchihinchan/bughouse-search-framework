#pragma once

#include "communication/message.h"
#include "game/types.h"
#include <vector>

// TODO
struct PredictionSummary {
  float expected_incoming = 0.f;
  float expected_outgoing = 0.f;

  float probability_receive_knight = 0.f;
  float probability_receive_bishop = 0.f;
  float probability_receive_rook = 0.f;
  float probability_receive_queen = 0.f;

  float attack_confidence = 0.f;
  float defence_confidence = 0.f;

  float volatility = 0.f;
};

struct PartnerContext {
  // board-derived
  int material_balance = 0;
  float king_danger = 0.f;
  int phase = 0;

  // message-derived
  PieceType requested = NO_PIECE_TYPE;
  bool danger = false;
  bool stall = false;
};

struct CommunicationContext {
  PartnerContext partner;
  PredictionSummary prediction;
};