#pragma once
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
  int active_player = -1;
  Clock::time_point turn_start;

  void set(int64_t base_ms, int inc_ms);
  void start(int player_id);
  void stop(int player_id);

  int64_t remaining(int player_id) const;
  bool flagged(int player_id) const;
  bool any_flagged() const;
};
