#pragma once

#include "agent/types.h"
#include <array>

struct ExperimentConfig {
  std::array<AgentConfig, PLAYER_NO> agent_configs{};
  uint64_t seed = 0;
};

class AgentStrategyExperiment {
public:
  explicit AgentStrategyExperiment(ExperimentConfig config);

  Agent &agent(PlayerId player) {
    return *agents_[to_int(player)];
  }
  const Agent &agent(PlayerId player) const {
    return *agents_[to_int(player)];
  }

  Channel &channel() { return channel_; }
  const ExperimentConfig &config() const { return config_; }

  AgentOutput choose_move(const BughouseState &game, PlayerId player,
                           const SearchLimits &limits,
                           std::stop_token stop_token) {
    return agent(player).choose_move(game, player, limits, stop_token);
  }

private:
  ExperimentConfig config_;
  Channel channel_;
  std::array<std::unique_ptr<Agent>, PLAYER_NO> agents_;
};