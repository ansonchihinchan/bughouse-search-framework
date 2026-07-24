#pragma once

#include "game/bughouse.h"

class Killer {
public:
  static constexpr int MAX_PLY = 128;

  void update(int ply, Move move) {
    if (move != killer1_[ply]) {
      killer2_[ply] = killer1_[ply];
      killer1_[ply] = move;
    }
  };

  Move first(int ply) const { return killer1_[ply]; }
  Move second(int ply) const { return killer2_[ply]; }
  
  void clear() {
    killer1_.fill(Move{});
    killer2_.fill(Move{});
  }

private:
  std::array<Move, MAX_PLY> killer1_{};
  std::array<Move, MAX_PLY> killer2_{};
};