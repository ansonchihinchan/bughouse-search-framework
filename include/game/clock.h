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
  std::array<PlayerId, BOARD_NO> active_players{NO_PLAYER, NO_PLAYER};
  std::array<Clock::time_point, BOARD_NO> turn_start{};

  void set(int64_t base_ms, int inc_ms);
  void start(PlayerId player_id);
  void stop(PlayerId player_id);

  int64_t remaining(PlayerId player_id) const;
  bool flagged(PlayerId player_id) const;
  bool any_flagged() const;

  PlayerId active_player(int board_idx) const {
    return active_players[board_idx];
  }
};