#pragma once
#include "types.h"
#include <array>

class Pocket {
public:
  // Stores pieces that can be dropped
  std::array<int, PIECE_TYPE_NO> pockets{};

  void add(PieceType pt) {
    if (pt >= PAWN && pt <= QUEEN)
      pockets[pt]++;
  }
  void remove(PieceType pt) {
    if (pockets[pt] > 0)
      pockets[pt]--;
  }
  int count(PieceType pt) const { return pockets[pt]; }
  bool contains(PieceType pt) const { return pockets[pt] > 0; }
  bool empty() const;
  void print() const;
};