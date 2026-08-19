#include "agent/experiment.h"
#include <utility>

AgentStrategyExperiment::AgentStrategyExperiment(ExperimentConfig config)
    : config_(std::move(config)) {
  for (int i = 0; i < PLAYER_NO; i++) {
    AgentConfig resolved = config_.agent_configs[i];
    resolved.seed = hash_combine(resolved.seed, config_.seed);
    agents_[i] = make_agent(resolved, &channel_);
  }
}