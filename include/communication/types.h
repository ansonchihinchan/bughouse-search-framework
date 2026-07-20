#pragma once

#include "game/board.h"
#include <optional>

struct PieceRequest {

  PieceType piece;

  int priority;
};

struct RecommendedMove {

  std::optional<Move> move;

  int confidence = 0;
};