#pragma once

#include "game/board.h"
#include <optional>
#include <vector>

class SharedInfo {
public:
  std::optional<Move> recommendedMove;

  std::optional<PieceType> requestedPiece;

  std::vector<PieceType> avoidGiving;

  std::vector<PieceType> recommendedCaptures;

  bool partnerUnderAttack = false;

  bool partnerHasMate = false;

  bool stall = false;

  void clear();
};