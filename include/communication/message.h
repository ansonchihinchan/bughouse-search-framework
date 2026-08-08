#pragma once

#include "game/types.h"
#include <optional>

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
  int eta_plies;
};

struct StrategyRequest {
  StrategyType strat = StrategyType::None;
  float confidence = 0.f;
  Urgency urgency = Urgency::Low;
};

struct Message {
  // Board full move count at root player's board
  int move_no = 0;

  std::optional<PieceRequest> piece_request;
  std::optional<StrategyRequest> strat_request;
};