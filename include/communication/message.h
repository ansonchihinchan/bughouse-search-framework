#pragma once

#include "game/types.h"

// Describe one side's output intent for the current real position

enum class StrategyType {
  None,

  AvoidTrades,
  TradeEverything,

  AttackNow,
  Defend,

  Stall,
  Flag
};

enum class Urgency { Low, Medium, High, Critical };

struct PieceRequest {
  PieceType piece = NO_PIECE_TYPE;
  float confidence = 0.f;
  Urgency urgency = Urgency::Low;
  int eta_plies = 0;
};

struct StrategyRequest {
  StrategyType strat = StrategyType::None;
  float confidence = 0.f;
  Urgency urgency = Urgency::Low;
};

struct Message {
  PlayerId sender = NO_PLAYER;
  int move_no = 0; // Board's full move count

  PieceRequest piece_request{};
  StrategyRequest strat_request{};
};