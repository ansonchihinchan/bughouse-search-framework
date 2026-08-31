#include <catch2/catch_all.hpp>

#include "agent/observer.h"
#include "agent/round_robin.h"
#include <sstream>

namespace {
ReplayEvent sample_event(bool with_message) {
  ReplayEvent event;
  event.event_index = 2;
  event.actor = to_player(0);
  event.board = 0;
  event.move = Move::normal(to_square(1, 0), to_square(2, 2));
  if (with_message) {
    Message message;
    message.sender = event.actor;
    message.move_no = 4;
    message.piece_request.piece = KNIGHT;
    message.piece_request.eta_plies = 2;
    message.piece_request.confidence = 0.75F;
    message.strat_request.strat = StrategyType::AttackNow;
    message.strat_request.confidence = 0.8F;
    event.message = message;
  }
  return event;
}
} // namespace

TEST_CASE("terminal renderer aligns both boards and state details",
          "[agent][observer][terminal][render]") {
  BughouseState state;
  state.position.pockets[0].add(KNIGHT);
  state.clock.set(61'234, 0);
  TerminalRenderer renderer;
  std::string first = renderer.render_game_state(state, sample_event(true));
  std::string second = renderer.render_game_state(state, sample_event(true));

  REQUIRE(first == second);
  REQUIRE(first.find("[Board A]           [Board B]") != std::string::npos);
  REQUIRE(first.find("8 r n b q k b n r   8 r n b q k b n r") !=
          std::string::npos);
  REQUIRE(first.find("  a b c d e f g h     a b c d e f g h") !=
          std::string::npos);
  REQUIRE(first.find("P0[Nx1]") != std::string::npos);
  REQUIRE(first.find("P0=1:01.234") != std::string::npos);
  REQUIRE(first.find("Ply 3: P0 board=A move=b1c3") != std::string::npos);
  REQUIRE(first.find("piece=N eta=2") != std::string::npos);
  REQUIRE(first.find("strategy=attack now") != std::string::npos);
}

TEST_CASE("terminal renderer safely reports a missing message",
          "[agent][observer][terminal][message]") {
  TerminalRenderer renderer;
  BughouseState state;
  std::string text = renderer.render_game_state(state, sample_event(false));
  REQUIRE(text.find("Message: none") != std::string::npos);
}

TEST_CASE("terminal renderer uses the authoritative stored clocks",
          "[agent][observer][terminal][clock]") {
  TerminalRenderer renderer;
  BughouseState state;
  state.clock.set(60'000, 0);
  state.clock.time_ms = {57'500, 60'000, 55'000, 59'250};
  std::string text = renderer.render_game_state(state);
  REQUIRE(text.find("P0=0:57.500") != std::string::npos);
  REQUIRE(text.find("P1=1:00.000") != std::string::npos);
  REQUIRE(text.find("P2=0:55.000") != std::string::npos);
  REQUIRE(text.find("P3=0:59.250") != std::string::npos);
}

TEST_CASE("terminal renderer reports tournament progress and final metrics",
          "[agent][observer][terminal][tournament]") {
  TerminalRenderer renderer;
  TournamentConfig config;
  config.game_count = 5;
  config.matchup.seed = 2026;
  config.matchup.agent_configs[1].type = AgentType::Request;
  TournamentGameRecord record;
  record.game_index = 1;
  record.seed = 99;
  TournamentSummary summary;
  summary.games = 2;
  summary.team_a_wins = 1;
  summary.draws = 1;
  summary.coordination_opportunities = 2;
  summary.synchrony_credit = COORDINATION_WINDOW_EVENTS;
  summary.total_drops = 4;
  summary.wasted_drops = 1;
  TournamentResult result;
  result.summary = summary;

  std::string start = renderer.render_tournament_start(config);
  std::string progress =
      renderer.render_tournament_progress(record, summary, config.game_count);
  std::string end = renderer.render_tournament_end(result);
  REQUIRE(start.find("Tournament seed=2026 games=5") != std::string::npos);
  REQUIRE(start.find("P1=request") != std::string::npos);
  REQUIRE(progress.find("Game 2 / 5 index=1 seed=99") != std::string::npos);
  REQUIRE(progress.find("Synchrony score=0.5") != std::string::npos);
  REQUIRE(progress.find("Wasted-drop rate=0.25 (1/4)") != std::string::npos);
  REQUIRE(end.find("tournament complete") != std::string::npos);
  REQUIRE(end.find("Games=2") != std::string::npos);
}

TEST_CASE("non-live observer output contains no terminal control sequences",
          "[agent][observer][terminal][non_live]") {
  std::ostringstream out;
  TerminalObserver observer(out, true);
  BughouseState state;
  observer.on_game_start(state);
  observer.on_ply(sample_event(false), state);
  REQUIRE(out.str().find("\x1b[") == std::string::npos);
}

TEST_CASE("tournament dispatch reaches both game-end overloads",
          "[agent][observer][terminal][dispatch]") {
  std::ostringstream out;
  TerminalObserver observer(out, false);
  TournamentConfig config;
  config.game_count = 1;
  config.self_play.max_plies = 0;
  config.observer = &observer;

  TournamentRunner{}.run(config);
  std::string text = out.str();
  REQUIRE(text.find("--- Self-play ---") != std::string::npos);
  REQUIRE(text.find("Result: ongoing plies=0") != std::string::npos);
  REQUIRE(text.find("Game 1 / 1") != std::string::npos);
  REQUIRE(text.find("--- tournament complete ---") != std::string::npos);
}

TEST_CASE("round-robin observer reports one aggregate tournament lifecycle",
          "[agent][observer][terminal][round_robin][regression]") {
  std::ostringstream out;
  TerminalObserver observer(out, false);
  RoundRobinConfig config;
  config.mode = ScheduleMode::Homogeneous;
  config.tournament.game_count = 1;
  config.tournament.self_play.max_plies = 0;
  config.tournament.observer = &observer;

  RoundRobinRunner{}.run(config);
  const std::string text = out.str();

  REQUIRE(
      text.find("Games=4 Team A wins=0 Team B wins=0 Draws=0 Unfinished=4") !=
      std::string::npos);
  REQUIRE(text.find("Game 4 / 4") != std::string::npos);
  REQUIRE(text.find("Games=1 Team A") == std::string::npos);
}