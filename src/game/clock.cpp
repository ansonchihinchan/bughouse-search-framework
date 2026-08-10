#include "game/clock.h"
#include "game/bughouse.h"

void BughouseClock::set(int64_t base_ms, int inc_ms) {
  time_ms.fill(base_ms);
  increment_ms = inc_ms;
  active_players.fill(NO_PLAYER);
}

void BughouseClock::start(PlayerId player_id) {
  int board_idx = board_of(player_id);
  active_players[board_idx] = player_id;
  turn_start[board_idx] = Clock::now();
}

void BughouseClock::stop(PlayerId player_id) {
  int board_idx = board_of(player_id);
  if (active_players[board_idx] != player_id)
    return;
  auto elapsed =
      std::chrono::duration_cast<ms>(Clock::now() - turn_start[board_idx])
          .count();
  time_ms[to_int(player_id)] -= elapsed;
  time_ms[to_int(player_id)] += increment_ms;
  active_players[board_idx] = NO_PLAYER;
}

int64_t BughouseClock::remaining(PlayerId player_id) const {
  int board_idx = board_of(player_id);
  if (active_players[board_idx] == player_id) {
    auto elapsed =
        std::chrono::duration_cast<ms>(Clock::now() - turn_start[board_idx])
            .count();
    return time_ms[to_int(player_id)] - elapsed;
  }
  return time_ms[to_int(player_id)];
}

bool BughouseClock::flagged(PlayerId player_id) const {
  return remaining(player_id) <= 0;
}

bool BughouseClock::any_flagged() const {
  for (int i = 0; i < PLAYER_NO; i++)
    if (flagged(to_player(i)))
      return true;
  return false;
}