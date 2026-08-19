#pragma once

#include "agent/experiment.h"
#include <array>
#include <cstddef>
#include <optional>
#include <stop_token>

enum class SelfPlayTermination {
  GameOver,
  PlyLimit,
  NoLegalMove,
  InvalidAgentMove,
  Stopped
};

struct SelfPlayConfig {
  SearchLimits search_limits{};
  size_t max_plies = 512;
  int first_board = 0;
};

struct SelfPlayResult {
  GameResult game_result = GameResult::ONGOING;
  SelfPlayTermination termination = SelfPlayTermination::PlyLimit;
  size_t plies = 0;
  size_t messages_sent = 0;
  std::array<size_t, PLAYER_NO> moves_by_player{};
  std::optional<PlayerId> invalid_player;
};

class SelfPlayRunner {
public:
  explicit SelfPlayRunner(AgentStrategyExperiment &experiment)
      : experiment_(experiment) {}

  SelfPlayResult run(BughouseState &game, const SelfPlayConfig &config,
                     std::stop_token stop_token = {});

private:
  AgentStrategyExperiment &experiment_;
};