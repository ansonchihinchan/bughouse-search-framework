#include <catch2/catch_all.hpp>

#include "agent/round_robin.h"

TEST_CASE("homogeneous schedule contains one explicit matchup per strategy",
          "[agent][round_robin][schedule]") {
  RoundRobinConfig config;
  config.mode = ScheduleMode::Homogeneous;
  std::vector<ExperimentConfig> schedule = generate_matchup_schedule(config);

  REQUIRE(schedule.size() == 4);
  for (size_t index = 0; index < schedule.size(); index++)
    for (const AgentConfig &seat : schedule[index].agent_configs)
      REQUIRE(seat.type == config.agent_types[index]);
}

TEST_CASE("exhaustive schedule enumerates all ordered four-seat assignments",
          "[agent][round_robin][schedule][exhaustive]") {
  RoundRobinConfig config;
  config.mode = ScheduleMode::ExhaustiveOrdered;
  std::vector<ExperimentConfig> first = generate_matchup_schedule(config);
  std::vector<ExperimentConfig> second = generate_matchup_schedule(config);

  REQUIRE(first.size() == 256);
  REQUIRE(second.size() == first.size());
  for (size_t i = 0; i < first.size(); i++) {
    REQUIRE(tournament_matchup_identity(first[i]) ==
            tournament_matchup_identity(second[i]));
    for (int player = 0; player < PLAYER_NO; player++)
      REQUIRE(first[i].agent_configs[player].type ==
              second[i].agent_configs[player].type);
  }
  REQUIRE(first.front().agent_configs[0].type == AgentType::Independent);
  REQUIRE(first.back().agent_configs[3].type == AgentType::Sacrifice);
}

TEST_CASE("explicit schedule preserves asymmetric PlayerId assignments",
          "[agent][round_robin][schedule][ablation]") {
  RoundRobinConfig config;
  config.mode = ScheduleMode::Explicit;
  ExperimentConfig matchup;
  matchup.seed = 91;
  matchup.agent_configs[0].type = AgentType::Independent;
  matchup.agent_configs[1].type = AgentType::Request;
  matchup.agent_configs[2].type = AgentType::SharedValue;
  matchup.agent_configs[3].type = AgentType::Sacrifice;
  config.explicit_matchups.push_back(matchup);

  std::vector<ExperimentConfig> schedule = generate_matchup_schedule(config);
  REQUIRE(schedule.size() == 1);
  REQUIRE(schedule[0].agent_configs[0].type == AgentType::Independent);
  REQUIRE(schedule[0].agent_configs[1].type == AgentType::Request);
  REQUIRE(schedule[0].agent_configs[2].type == AgentType::SharedValue);
  REQUIRE(schedule[0].agent_configs[3].type == AgentType::Sacrifice);

  ExperimentConfig changed = matchup;
  changed.agent_configs[0].type = AgentType::Request;
  REQUIRE(tournament_game_seed(changed, 0) !=
          tournament_game_seed(matchup, 0));
  REQUIRE(tournament_game_seed(matchup, 0) !=
          tournament_game_seed(matchup, 1));
}

TEST_CASE("round-robin execution delegates each scheduled matchup",
          "[agent][round_robin][execution][determinism]") {
  RoundRobinConfig config;
  config.mode = ScheduleMode::Homogeneous;
  config.agent_types = {AgentType::Independent, AgentType::Request};
  config.tournament.game_count = 1;
  config.tournament.matchup.seed = 1234;
  config.tournament.self_play.max_plies = 1;
  config.tournament.self_play.search_limits.max_depth = 1;

  RoundRobinResult first = RoundRobinRunner{}.run(config);
  RoundRobinResult second = RoundRobinRunner{}.run(config);
  REQUIRE(first.schedule.size() == 2);
  REQUIRE(first.matchups.size() == 2);
  REQUIRE(second.matchups.size() == first.matchups.size());
  for (size_t i = 0; i < first.matchups.size(); i++) {
    REQUIRE(first.matchups[i].games.size() == 1);
    REQUIRE(first.matchups[i].games[0].seed ==
            second.matchups[i].games[0].seed);
    REQUIRE(first.matchups[i].games[0].result.plies ==
            second.matchups[i].games[0].result.plies);
  }
  REQUIRE(first.matchups[0].games[0].seed !=
          first.matchups[1].games[0].seed);
}