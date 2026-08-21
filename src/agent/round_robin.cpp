#include "agent/round_robin.h"
#include <stdexcept>

std::vector<ExperimentConfig>
generate_matchup_schedule(const RoundRobinConfig &config) {
  if (config.mode == ScheduleMode::Explicit)
    return config.explicit_matchups;

  if (config.agent_types.empty())
    throw std::invalid_argument("schedule requires at least one agent type");

  std::vector<ExperimentConfig> schedule;
  if (config.mode == ScheduleMode::Homogeneous) {
    schedule.reserve(config.agent_types.size());
    for (AgentType type : config.agent_types) {
      ExperimentConfig matchup = config.tournament.matchup;
      for (AgentConfig &agent : matchup.agent_configs)
        agent.type = type;
      schedule.push_back(std::move(matchup));
    }
    return schedule;
  }

  size_t count = config.agent_types.size();
  schedule.reserve(count * count * count * count);
  for (AgentType p0 : config.agent_types)
    for (AgentType p1 : config.agent_types)
      for (AgentType p2 : config.agent_types)
        for (AgentType p3 : config.agent_types) {
          ExperimentConfig matchup = config.tournament.matchup;
          matchup.agent_configs[0].type = p0;
          matchup.agent_configs[1].type = p1;
          matchup.agent_configs[2].type = p2;
          matchup.agent_configs[3].type = p3;
          schedule.push_back(std::move(matchup));
        }
  return schedule;
}

RoundRobinResult RoundRobinRunner::run(const RoundRobinConfig &config,
                                       std::stop_token stop_token) const {
  RoundRobinResult result;
  result.schedule = generate_matchup_schedule(config);
  result.matchups.reserve(result.schedule.size());
  TournamentRunner runner;

  for (const ExperimentConfig &matchup : result.schedule) {
    if (stop_token.stop_requested())
      break;
    TournamentConfig tournament = config.tournament;
    tournament.matchup = matchup;
    result.matchups.push_back(runner.run(tournament, stop_token));
  }
  
  return result;
}