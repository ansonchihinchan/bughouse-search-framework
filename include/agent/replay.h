#pragma once

#include "communication/message.h"
#include "game/bughouse.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <ostream>
#include <vector>

inline constexpr size_t COORDINATION_WINDOW_EVENTS = 4;

struct ReplayEvent {
  size_t event_index = 0;
  PlayerId actor = NO_PLAYER;
  int board = 0;
  Move move{};
  std::array<Pocket, PLAYER_NO> pockets_after{};
  std::array<int64_t, PLAYER_NO> clocks_after_ms{};
  std::optional<Message> message;
};

struct GameReplay {
  BughouseState initial_state{};
  std::vector<ReplayEvent> events;
  GameResult terminal_result = GameResult::ONGOING;
};

struct ReplayMetrics {
  size_t coordination_opportunities = 0;
  size_t coordinated_responses = 0;
  size_t synchrony_credit = 0;
  size_t total_drops = 0;
  size_t wasted_drops = 0;

  double synchrony_score() const;
  double wasted_drop_rate() const;
};

ReplayMetrics analyse_replay(const GameReplay &replay);
void write_replay(std::ostream &out, const GameReplay &replay);
GameReplay read_replay(std::istream &in);
void view_replay(std::ostream &out, const GameReplay &replay,
                 bool step_by_step = false, std::istream *step_input = nullptr);