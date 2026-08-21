#pragma once

#include "agent/experiment.h"
#include <array>
#include <cstddef>
#include <optional>
#include <stop_token>

inline constexpr size_t STRATEGY_TYPE_COUNT =
    static_cast<size_t>(StrategyType::Flag) + 1;

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
  int64_t simulated_move_cost_ms = 1000;
};

struct SelfPlayResult {
  GameResult game_result = GameResult::ONGOING;
  SelfPlayTermination termination = SelfPlayTermination::PlyLimit;
  size_t plies = 0;
  size_t messages_sent = 0;
  size_t piece_requests_generated = 0;
  size_t strategy_requests_generated = 0;
  size_t requests_fulfilled = 0;
  size_t piece_transfers = 0;
  std::array<size_t, STRATEGY_TYPE_COUNT> messages_by_strategy{};
  std::array<size_t, PLAYER_NO> moves_by_player{};
  std::array<int64_t, PLAYER_NO> final_clocks_ms{};
  std::array<AgentType, PLAYER_NO> strategies{};
  AgentOutput::Metrics agent_metrics{};
  size_t accepted_sacrifice_transfers = 0;
  size_t accepted_sacrifice_transfers_used = 0;
  size_t successful_temporal_sacrifices = 0;
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