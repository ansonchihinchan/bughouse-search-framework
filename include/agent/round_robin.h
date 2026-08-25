#pragma once

#include "agent/tournament.h"
#include <ostream>
#include <stop_token>
#include <vector>

enum class ScheduleMode { Homogeneous, ExhaustiveOrdered, Explicit };

struct RoundRobinConfig {
  TournamentConfig tournament{};
  ScheduleMode mode = ScheduleMode::Homogeneous;
  std::vector<AgentType> agent_types{AgentType::Independent, AgentType::Request,
                                     AgentType::SharedValue,
                                     AgentType::Sacrifice};
  std::vector<ExperimentConfig> explicit_matchups;
};

struct RoundRobinResult {
  std::vector<ExperimentConfig> schedule;
  std::vector<TournamentResult> matchups;
  std::vector<TournamentGameRecord> games;
  TournamentSummary summary{};
};

std::vector<ExperimentConfig>
generate_matchup_schedule(const RoundRobinConfig &config);

class RoundRobinRunner {
public:
  RoundRobinResult run(const RoundRobinConfig &config,
                       std::stop_token stop_token = {}) const;
};

void write_round_robin_csv(std::ostream &out, const RoundRobinResult &result);