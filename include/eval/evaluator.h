#pragma once

#include "game/bughouse.h"
#include "search/types.h"

// Abstract Evaluator interface
class Evaluator {
public:
  // Destructor
  virtual ~Evaluator() = default;

  // Static score for the joint position and context(player_id, clock) from
  // player_id's team's perspective.
  virtual int evaluate(const BughousePosition &position,
                       const SearchContext &context) const = 0;

  // Optional hook for leaf-extension searches
  // A noisy position is one where something significant is happening.
  virtual bool is_noisy(const BughousePosition &position,
                        const SearchContext &context) const {
    (void)position;
    (void)context;
    return false;
  }
};