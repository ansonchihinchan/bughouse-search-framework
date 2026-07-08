#pragma once

#include "game/types.h"
#include <array>
#include <chrono>
#include <cstdint>

using ms = std::chrono::milliseconds;
using Clock = std::chrono::steady_clock;

class BughouseClock {
public:
  // time_ms[i] = remaining milliseconds for player i
  std::array<int64_t, 4> time_ms{};

  int increment_ms = 0;
  PlayerId active_player = NO_PLAYER;
  Clock::time_point turn_start;

  void set(int64_t base_ms, int inc_ms);
  void start(PlayerId player_id);
  void stop(PlayerId player_id);

  int64_t remaining(PlayerId player_id) const;
  bool flagged(PlayerId player_id) const;
  bool any_flagged() const;
};