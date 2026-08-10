#pragma once

#include "eval/context.h"
#include "game/bughouse.h"

// Abstract Evaluator interface
class Evaluator {
public:
  // Destructor
  virtual ~Evaluator() = default;

  // Static score for the joint position and context(player_id, clock) from
  // player_id's team's perspective.
  virtual int evaluate(const BughousePosition &position, PlayerId root_player,
                       const std::array<int64_t, PLAYER_NO> &remaining,
                       const CommunicationContext &comm_context) const = 0;

  // Optional hook for leaf-extension searches
  // A noisy position is one where something significant is happening.
  virtual bool is_noisy(const BughousePosition &position,
                        PlayerId root_player) const {
    (void)position;
    (void)root_player;
    return false;
  }

  virtual float volatility(const BughousePosition &position,
                           PlayerId root_player) const {
    (void)position;
    (void)root_player;
    return 0.0f;
  }
};