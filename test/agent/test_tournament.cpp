#include <catch2/catch_all.hpp>

#include "agent/tournament.h"
#include <sstream>

namespace {
TournamentConfig short_tournament(size_t games = 1, size_t plies = 1) {
  TournamentConfig config;
  config.game_count = games;
  config.matchup.seed = 2026;
  for (AgentConfig &agent : config.matchup.agent_configs) {
    agent.type = AgentType::Independent;
    agent.search = SearchAlgorithm::PVS;
  }
  config.self_play.max_plies = plies;
  config.self_play.search_limits.max_depth = 1;
  config.self_play.simulated_move_cost_ms = 1000;
  return config;
}

BughouseState causal_sacrifice_game() {
  BughouseState game;
  game.position.boards[0].load_fen("3r2k1/3n4/8/8/8/8/8/3Q2K1 w - - 0 1");
  game.position.boards[1].load_fen("4PPPk/4PKN1/4PPQ1/8/8/8/8/8 b - - 0 1");
  game.history = {RepetitionNode{position_hash(game.position), 0, 0}};
  return game;
}

void require_equal_game(const TournamentGameRecord &a,
                        const TournamentGameRecord &b) {
  REQUIRE(a.game_index == b.game_index);
  REQUIRE(a.seed == b.seed);
  REQUIRE(a.winning_team == b.winning_team);
  REQUIRE(a.result.game_result == b.result.game_result);
  REQUIRE(a.result.termination == b.result.termination);
  REQUIRE(a.result.plies == b.result.plies);
  REQUIRE(a.result.final_clocks_ms == b.result.final_clocks_ms);
  REQUIRE(a.result.moves_by_player == b.result.moves_by_player);
  REQUIRE(a.result.strategies == b.result.strategies);
  REQUIRE(a.result.messages_sent == b.result.messages_sent);
  REQUIRE(a.result.requests_fulfilled == b.result.requests_fulfilled);
  REQUIRE(a.result.agent_metrics.sacrifice_attempts ==
          b.result.agent_metrics.sacrifice_attempts);
}
} // namespace

TEST_CASE("tournament runs the configured number of deterministic games",
          "[agent][tournament][reproducibility]") {
  TournamentConfig config = short_tournament(2);
  TournamentResult first = TournamentRunner{}.run(config);
  TournamentResult second = TournamentRunner{}.run(config);

  REQUIRE(first.games.size() == 2);
  REQUIRE(second.games.size() == 2);
  REQUIRE(first.summary.games == 2);
  REQUIRE(first.summary.total_plies == 2);
  REQUIRE(first.games[0].seed != first.games[1].seed);
  REQUIRE(first.games[0].seed == tournament_game_seed(config.matchup, 0));
  REQUIRE(first.games[1].seed == tournament_game_seed(config.matchup, 1));
  require_equal_game(first.games[0], second.games[0]);
  require_equal_game(first.games[1], second.games[1]);
}

TEST_CASE("tournament preserves explicit asymmetric seat assignments",
          "[agent][tournament][matchup][ablation]") {
  TournamentConfig config = short_tournament(1, 0);
  config.matchup.agent_configs[0].type = AgentType::Independent;
  config.matchup.agent_configs[1].type = AgentType::Request;
  config.matchup.agent_configs[2].type = AgentType::SharedValue;
  config.matchup.agent_configs[3].type = AgentType::Sacrifice;

  TournamentResult result = TournamentRunner{}.run(config);

  REQUIRE(result.games[0].result.strategies ==
          std::array{AgentType::Independent, AgentType::Request,
                     AgentType::SharedValue, AgentType::Sacrifice});

  ExperimentConfig ablation = config.matchup;
  ablation.agent_configs[3].type = AgentType::Independent;
  REQUIRE(tournament_matchup_identity(ablation) !=
          tournament_matchup_identity(config.matchup));
}

TEST_CASE("tournament preserves Request communication metrics",
          "[agent][tournament][request][metrics]") {
  TournamentConfig config = short_tournament();
  config.matchup.agent_configs[0].type = AgentType::Request;

  TournamentResult result = TournamentRunner{}.run(config);
  const SelfPlayResult &game = result.games[0].result;

  REQUIRE(game.messages_sent == 1);
  REQUIRE(game.piece_requests_generated == 1);
  REQUIRE(game.strategy_requests_generated == 1);
  REQUIRE(result.summary.messages_sent == 1);
}

TEST_CASE("tournament preserves modeled and actual Sacrifice metrics",
          "[agent][tournament][sacrifice][metrics]") {
  TournamentConfig config = short_tournament(1, 2);
  config.initial_state = causal_sacrifice_game();
  config.matchup.agent_configs[0].type = AgentType::Sacrifice;
  config.matchup.agent_configs[2].search_params.quiescence_enabled = false;
  config.self_play.search_limits.max_depth = 2;

  TournamentResult result = TournamentRunner{}.run(config);
  const SelfPlayResult &game = result.games[0].result;

  REQUIRE(game.agent_metrics.sacrifice_attempts >= 1);
  REQUIRE(game.agent_metrics.sacrifices_accepted == 1);
  REQUIRE(game.agent_metrics.temporal_partner_uses >= 1);
  REQUIRE(game.accepted_sacrifice_transfers_used == 1);
  REQUIRE(game.successful_temporal_sacrifices == 1);
  REQUIRE(result.summary.sacrifices_accepted == 1);
  REQUIRE(result.summary.actual_sacrifice_uses == 1);
}

TEST_CASE("tournament records terminal games and configured clocks",
          "[agent][tournament][terminal][clock]") {
  TournamentConfig config = short_tournament();
  config.initial_state.position.boards[0].load_fen(
      "7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");
  config.initial_state.clock.set(42'000, 500);

  TournamentResult result = TournamentRunner{}.run(config);
  const TournamentGameRecord &game = result.games[0];

  REQUIRE(game.result.game_result == GameResult::TEAM_A_WINS);
  REQUIRE(game.winning_team == 0);
  REQUIRE(game.result.plies == 0);
  REQUIRE(game.result.final_clocks_ms ==
          std::array<int64_t, PLAYER_NO>{42'000, 42'000, 42'000, 42'000});
  REQUIRE(result.summary.team_a_wins == 1);
}

TEST_CASE("tournament CSV contains raw results and reproduction metadata",
          "[agent][tournament][csv]") {
  TournamentConfig config = short_tournament();
  config.matchup.agent_configs[0].type = AgentType::Request;
  TournamentResult result = TournamentRunner{}.run(config);
  std::ostringstream csv;

  write_tournament_csv(csv, result);
  std::string text = csv.str();

  REQUIRE(text.find("game_index,seed,base_seed,matchup_identity,result") == 0);
  REQUIRE(text.find("agent0_config") != std::string::npos);
  REQUIRE(text.find("initial_clock0_ms") != std::string::npos);
  REQUIRE(text.find("fulfilled_requests") != std::string::npos);
  REQUIRE(text.find("actual_partner_uses") != std::string::npos);
  REQUIRE(text.find(std::to_string(result.games[0].seed)) != std::string::npos);
  REQUIRE(text.find("request;pvs") != std::string::npos);
}