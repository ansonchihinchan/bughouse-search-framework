#include <catch2/catch_all.hpp>

#include "agent/self_play.h"
#include <string>

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

char white_piece_fen(PieceType piece) {
  switch (piece) {
  case PAWN:
    return 'P';
  case KNIGHT:
    return 'N';
  case BISHOP:
    return 'B';
  case ROOK:
    return 'R';
  case QUEEN:
    return 'Q';
  default:
    return 'P';
  }
}

BughouseState causal_sacrifice_game() {
  BughouseState game;
  game.position.boards[0].load_fen("3r2k1/3n4/8/8/8/8/8/3Q2K1 w - - 0 1");
  game.position.boards[1].load_fen("4PPPk/4PKN1/4PPQ1/8/8/8/8/8 b - - 0 1");
  game.history = {RepetitionNode{position_hash(game.position), 0, 0}};
  return game;
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
  for (AgentType strategy : result.strategies)
    REQUIRE(strategy == AgentType::Independent);
  REQUIRE(result.final_clocks_ms[0] == DEFAULT_TIME + DEFAULT_INCREMENT - 1000);
  REQUIRE(result.final_clocks_ms[1] == DEFAULT_TIME + DEFAULT_INCREMENT - 1000);
  REQUIRE(result.final_clocks_ms[2] == DEFAULT_TIME + DEFAULT_INCREMENT - 1000);
  REQUIRE(result.final_clocks_ms[3] == DEFAULT_TIME + DEFAULT_INCREMENT - 1000);
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
  REQUIRE(result.piece_requests_generated == 1);
  REQUIRE(result.strategy_requests_generated == 1);
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
  REQUIRE(first.final_clocks_ms == second.final_clocks_ms);
  REQUIRE(first.strategies == second.strategies);
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

TEST_CASE("self-play records mixed strategy identity",
          "[agent][self_play][experiment]") {
  ExperimentConfig config = independent_roster(44);
  config.agent_configs[0].type = AgentType::Independent;
  config.agent_configs[1].type = AgentType::Request;
  config.agent_configs[2].type = AgentType::SharedValue;
  config.agent_configs[3].type = AgentType::Sacrifice;
  AgentStrategyExperiment experiment(config);
  SelfPlayRunner runner(experiment);
  BughouseState game;

  SelfPlayResult result = runner.run(game, shallow_game(0));

  REQUIRE(result.strategies ==
          std::array{AgentType::Independent, AgentType::Request,
                     AgentType::SharedValue, AgentType::Sacrifice});
}

TEST_CASE("self-play records fulfilled piece requests",
          "[agent][self_play][communication][metrics]") {
  ExperimentConfig probe_config = independent_roster();
  probe_config.agent_configs[0].type = AgentType::Request;
  AgentStrategyExperiment probe_experiment(probe_config);
  SelfPlayRunner probe_runner(probe_experiment);
  BughouseState probe_game;
  probe_game.position.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  REQUIRE(probe_runner.run(probe_game, shallow_game(1)).messages_sent == 1);
  PieceType requested =
      probe_experiment.channel().latest(to_player(0)).piece_request.piece;
  REQUIRE(requested != NO_PIECE_TYPE);

  ExperimentConfig config = independent_roster();
  config.agent_configs[0].type = AgentType::Request;
  AgentStrategyExperiment experiment(config);
  SelfPlayRunner runner(experiment);
  BughouseState game;
  game.position.boards[0].load_fen("4k3/8/8/8/8/8/8/4K3 w - - 0 1");
  std::string partner_fen = "4k3/8/8/8/8/8/4";
  partner_fen += white_piece_fen(requested);
  partner_fen += "3/4r1K1 b - - 0 1";
  game.position.boards[1].load_fen(partner_fen);
  game.history = {RepetitionNode{position_hash(game.position), 0, 0}};

  SelfPlayResult result = runner.run(game, shallow_game(2));

  REQUIRE(result.piece_requests_generated == 1);
  REQUIRE(result.piece_transfers == 1);
  REQUIRE(result.requests_fulfilled == 1);
}

TEST_CASE("self-play records successful temporal sacrifices",
          "[agent][self_play][sacrifice][metrics]") {
  ExperimentConfig config = independent_roster();
  config.agent_configs[0].type = AgentType::Sacrifice;
  config.agent_configs[2].search_params.quiescence_enabled = false;
  AgentStrategyExperiment experiment(config);
  SelfPlayRunner runner(experiment);
  BughouseState game = causal_sacrifice_game();
  SelfPlayConfig run_config = shallow_game(2);
  run_config.search_limits.max_depth = 2;

  SelfPlayResult result = runner.run(game, run_config);

  REQUIRE(result.agent_metrics.sacrifice_attempts >= 1);
  REQUIRE(result.agent_metrics.sacrifices_accepted == 1);
  REQUIRE(result.agent_metrics.temporal_transfers_observed >= 1);
  REQUIRE(result.agent_metrics.temporal_partner_uses >= 1);
  REQUIRE(result.accepted_sacrifice_transfers == 1);
  REQUIRE(result.accepted_sacrifice_transfers_used == 1);
  REQUIRE(result.successful_temporal_sacrifices == 1);
  REQUIRE_FALSE(game.position.pockets[2].contains(KNIGHT));
}

TEST_CASE("self-play flags deterministically before an unaffordable move",
          "[agent][self_play][clock][termination]") {
  AgentStrategyExperiment experiment(independent_roster());
  SelfPlayRunner runner(experiment);
  BughouseState game;
  game.clock.time_ms[0] = 500;

  SelfPlayResult result = runner.run(game, shallow_game(1));

  REQUIRE(result.termination == SelfPlayTermination::GameOver);
  REQUIRE(result.game_result == GameResult::TEAM_B_WINS);
  REQUIRE(result.plies == 0);
  REQUIRE(result.final_clocks_ms[0] == 0);
}