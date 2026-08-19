#include <catch2/catch_all.hpp>

#include "agent/self_play.h"

namespace {
ExperimentConfig independent_roster(uint64_t seed = 0) {
  ExperimentConfig config;
  config.seed = seed;
  for (AgentConfig &agent : config.agent_configs) {
    agent.type = AgentType::Independent;
    agent.search = SearchAlgorithm::PVS;
  }
  return config;
}

SelfPlayConfig shallow_game(size_t max_plies) {
  SelfPlayConfig config;
  config.search_limits.max_depth = 1;
  config.max_plies = max_plies;
  return config;
}
} // namespace

TEST_CASE("self-play schedules all four agents and applies legal moves",
          "[agent][self_play]") {
  AgentStrategyExperiment experiment(independent_roster(17));
  SelfPlayRunner runner(experiment);
  BughouseState game;

  SelfPlayResult result = runner.run(game, shallow_game(4));

  REQUIRE(result.termination == SelfPlayTermination::PlyLimit);
  REQUIRE(result.plies == 4);
  REQUIRE(game.history.size() == 5);
  for (size_t count : result.moves_by_player)
    REQUIRE(count == 1);
  REQUIRE_FALSE(result.invalid_player.has_value());
}

TEST_CASE("self-play delivers request-agent communication",
          "[agent][self_play][communication]") {
  ExperimentConfig config = independent_roster();
  config.agent_configs[0].type = AgentType::Request;
  AgentStrategyExperiment experiment(config);
  SelfPlayRunner runner(experiment);
  BughouseState game;

  SelfPlayResult result = runner.run(game, shallow_game(1));

  REQUIRE(result.messages_sent == 1);
  Message delivered = experiment.channel().latest(to_player(0));
  REQUIRE(delivered.sender == to_player(0));
  REQUIRE(delivered.move_no == game.position.boards[0].fullMove);
  REQUIRE(delivered.piece_request.piece != NO_PIECE_TYPE);
}

TEST_CASE("equal self-play configurations produce equal games",
          "[agent][self_play][reproducibility]") {
  AgentStrategyExperiment first_experiment(independent_roster(2026));
  AgentStrategyExperiment second_experiment(independent_roster(2026));
  SelfPlayRunner first_runner(first_experiment);
  SelfPlayRunner second_runner(second_experiment);
  BughouseState first_game;
  BughouseState second_game;

  SelfPlayResult first = first_runner.run(first_game, shallow_game(8));
  SelfPlayResult second = second_runner.run(second_game, shallow_game(8));

  REQUIRE(first.termination == second.termination);
  REQUIRE(first.game_result == second.game_result);
  REQUIRE(first.plies == second.plies);
  REQUIRE(first.moves_by_player == second.moves_by_player);
  REQUIRE(first_game.position.boards == second_game.position.boards);
  REQUIRE(first_game.position.pockets == second_game.position.pockets);
}

TEST_CASE("self-play reports an already terminal game without searching",
          "[agent][self_play][termination]") {
  AgentStrategyExperiment experiment(independent_roster());
  SelfPlayRunner runner(experiment);
  BughouseState game;
  game.position.boards[0].load_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

  SelfPlayResult result = runner.run(game, shallow_game(8));

  REQUIRE(result.termination == SelfPlayTermination::GameOver);
  REQUIRE(result.game_result == GameResult::TEAM_A_WINS);
  REQUIRE(result.plies == 0);
  REQUIRE(game.history.size() == 1);
}