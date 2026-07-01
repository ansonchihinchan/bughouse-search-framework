#include "clock.h"
#include "bughouse.h"

void BughouseClock::set(int64_t base_ms, int inc_ms) {
  time_ms.fill(base_ms);
  increment_ms = inc_ms;
  active_player = -1;
}

void BughouseClock::start(int player_id) {
  active_player = player_id;
  turn_start = Clock::now();
}

void BughouseClock::stop(int player_id) {
  if (active_player != player_id)
    return;
  auto elapsed =
      std::chrono::duration_cast<ms>(Clock::now() - turn_start).count();
  time_ms[player_id] -= elapsed;
  time_ms[player_id] += increment_ms;
  active_player = -1;
}

int64_t BughouseClock::remaining(int player_id) const {
  if (active_player == player_id) {
    auto elapsed =
        std::chrono::duration_cast<ms>(Clock::now() - turn_start).count();
    return time_ms[player_id] - elapsed;
  }
  return time_ms[player_id];
}

bool BughouseClock::flagged(int player_id) const {
  return remaining(player_id) <= 0;
}
bool BughouseClock::any_flagged() const {
  for (int i = 0; i < PLAYER_NO; i++)
    if (flagged(i))
      return true;
  return false;
}