// include/search/evaluation.h
#pragma once
#include "game/bughouse.h"

// Abstract Evaluator interface
class Evaluator {
public:
  // Destructor
  virtual ~Evaluator() = default;

  // Static score for the joint position (both boards, all four
  // pockets, optionally both clocks) from player_id's team's perspective.
  //
  // For any p, q on opposing teams,
  // evaluate(state, p) == -evaluate(state, q).
  virtual int evaluate(const BughouseState &state, int player_id) const = 0;

  // Optional hook for leaf-extension searches
  // A noisy position is one where something significant is happening.
  virtual bool is_noisy(const BughouseState &state, int player_id) const {
    (void)state;
    (void)player_id;
    return false;
  }
};