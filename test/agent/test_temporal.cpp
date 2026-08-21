#include <catch2/catch_all.hpp>

#include "agent/temporal.h"
#include "agent/types.h"
#include "game/movegen.h"

namespace {
TemporalState initial_temporal_state(int preferred_board = 0) {
  BughouseState game;
  TemporalState state;
  state.position = game.position;
  state.remaining_ms.fill(60'000);
  state.preferred_board = preferred_board;
  state.events_remaining = 4;
  state.root_player = to_player(0);
  state.increment_ms = 2'000;
  return state;
}

BughouseState causal_sacrifice_game() {
  BughouseState game;
  game.position.boards[0].load_fen("3r2k1/3n4/8/8/8/8/8/3Q2K1 w - - 0 1");
  game.position.boards[1].load_fen("4PPPk/4PKN1/4PPQ1/8/8/8/8/8 b - - 0 1");
  game.history = {RepetitionNode{position_hash(game.position), 0, 0}};
  return game;
}

SearchResult independent_baseline(const BughouseState &game,
                                  const SearchLimits &limits,
                                  std::stop_token stop_token) {
  AgentConfig config;
  config.type = AgentType::Independent;
  return make_agent(config)
      ->choose_move(game, to_player(0), limits, stop_token)
      .search_result;
}
} // namespace

TEST_CASE("temporal coordinator selects a causal resource sacrifice",
          "[agent][temporal][sacrifice]") {
  BughouseState game = causal_sacrifice_game();
  SearchLimits limits;
  limits.max_depth = 2;
  std::stop_source stop;
  SearchResult baseline = independent_baseline(game, limits, stop.get_token());

  TemporalCoordinator coordinator;
  SearchResult selected = coordinator.select_move(
      game, to_player(0), baseline, limits, stop.get_token(),
      std::chrono::steady_clock::now());
  const TemporalDecision &decision = coordinator.last_decision();

  INFO("events=" << decision.stats.rollout_events
                 << " searches=" << decision.stats.local_search_calls
                 << " elapsed_ms=" << decision.stats.elapsed.count()
                 << " baseline_team=" << decision.baseline_trace.team_score
                 << " candidate_team="
                 << decision.last_candidate_trace.team_score << " partner_used="
                 << decision.last_candidate_trace.partner_used_transfer);
  for (size_t i = 0; i < decision.last_candidate_trace.event_count; i++)
    UNSCOPED_INFO(
        "event " << i << " actor="
                 << to_int(decision.last_candidate_trace.events[i].actor)
                 << " move="
                 << decision.last_candidate_trace.events[i].move.to_string());

  REQUIRE(decision.selected_sacrifice);
  REQUIRE(selected.best_move == Move::normal(to_square(3, 0), to_square(3, 6)));
  REQUIRE(decision.selected_trace.partner_used_transfer);
  REQUIRE(decision.selected_trace.events[0].transferred_piece == KNIGHT);
  bool found_partner_drop = false;
  for (size_t i = 0; i < decision.selected_trace.event_count; i++) {
    const TemporalEvent &event = decision.selected_trace.events[i];
    if (event.actor == to_player(2) && event.move.is_drop() &&
        event.move.drop_pt == KNIGHT && event.tracked_transfer_consumed)
      found_partner_drop = true;
  }
  REQUIRE(found_partner_drop);
  REQUIRE(decision.selected_local_score <=
          decision.baseline_local_score -
              coordinator.config().local_sacrifice_margin);
  REQUIRE(decision.selected_trace.team_score >=
          decision.baseline_trace.team_score +
              coordinator.config().temporal_gain_margin);
}

TEST_CASE("sacrifice classification requires every causal condition",
          "[agent][temporal][sacrifice]") {
  TemporalConfig config;
  SacrificeEvidence evidence{300, 150, 400, 500, true, true, true};
  REQUIRE(qualifies_sacrifice(evidence, config));

  SECTION("a locally improving move is not a sacrifice") {
    evidence.candidate_local_score = 350;
    REQUIRE_FALSE(qualifies_sacrifice(evidence, config));
  }
  SECTION("static pocket gain without partner use is insufficient") {
    evidence.partner_used_transfer = false;
    REQUIRE_FALSE(qualifies_sacrifice(evidence, config));
  }
  SECTION("insufficient temporal gain is rejected") {
    evidence.candidate_team_score = 449;
    REQUIRE_FALSE(qualifies_sacrifice(evidence, config));
  }
  SECTION("a piece already owned by the partner is not causal") {
    evidence.causal_availability = false;
    REQUIRE_FALSE(qualifies_sacrifice(evidence, config));
  }
  SECTION("a move without a resource transfer is rejected") {
    evidence.resource_transferred = false;
    REQUIRE_FALSE(qualifies_sacrifice(evidence, config));
  }
}

TEST_CASE("coordinator falls back when the partner cannot use the transfer",
          "[agent][temporal][sacrifice]") {
  BughouseState game = causal_sacrifice_game();
  SearchLimits limits;
  limits.max_depth = 2;
  std::stop_source stop;
  SearchResult baseline = independent_baseline(game, limits, stop.get_token());
  TemporalConfig config;
  config.rollout_events = 0;
  TemporalCoordinator coordinator(config);

  SearchResult selected = coordinator.select_move(
      game, to_player(0), baseline, limits, stop.get_token(),
      std::chrono::steady_clock::now());

  REQUIRE_FALSE(coordinator.last_decision().selected_sacrifice);
  REQUIRE(selected.best_move == baseline.best_move);
}

TEST_CASE("an interrupted coordinator returns the completed baseline",
          "[agent][temporal][interrupt]") {
  BughouseState game = causal_sacrifice_game();
  SearchLimits limits;
  limits.max_depth = 2;
  std::stop_source baseline_stop;
  SearchResult baseline =
      independent_baseline(game, limits, baseline_stop.get_token());
  std::stop_source interrupted;
  interrupted.request_stop();
  TemporalCoordinator coordinator;

  SearchResult selected = coordinator.select_move(
      game, to_player(0), baseline, limits, interrupted.get_token(),
      std::chrono::steady_clock::now());

  REQUIRE(selected.best_move == baseline.best_move);
  REQUIRE(selected.score == baseline.score);
  REQUIRE_FALSE(coordinator.last_decision().selected_sacrifice);
}

TEST_CASE("temporal coordinator is deterministic",
          "[agent][temporal][rollout]") {
  BughouseState game = causal_sacrifice_game();
  SearchLimits limits;
  limits.max_depth = 2;
  std::stop_source stop;
  SearchResult baseline = independent_baseline(game, limits, stop.get_token());
  TemporalCoordinator first;
  TemporalCoordinator second;

  SearchResult first_result =
      first.select_move(game, to_player(0), baseline, limits, stop.get_token(),
                        std::chrono::steady_clock::now());
  SearchResult second_result =
      second.select_move(game, to_player(0), baseline, limits, stop.get_token(),
                         std::chrono::steady_clock::now());

  REQUIRE(first_result.best_move == second_result.best_move);
  REQUIRE(first_result.score == second_result.score);
  const TemporalTrace &a = first.last_decision().selected_trace;
  const TemporalTrace &b = second.last_decision().selected_trace;
  REQUIRE(a.event_count == b.event_count);
  REQUIRE(a.team_score == b.team_score);
  for (size_t i = 0; i < a.event_count; i++) {
    REQUIRE(a.events[i].actor == b.events[i].actor);
    REQUIRE(a.events[i].move == b.events[i].move);
  }
}

TEST_CASE("SacrificeAgent uses the coordinator without communication",
          "[agent][temporal][integration]") {
  BughouseState game = causal_sacrifice_game();
  AgentConfig config;
  config.type = AgentType::Sacrifice;
  auto agent = make_agent(config);
  SearchLimits limits;
  limits.max_depth = 2;
  std::stop_source stop;

  AgentOutput output =
      agent->choose_move(game, to_player(0), limits, stop.get_token());

  REQUIRE(output.search_result.best_move ==
          Move::normal(to_square(3, 0), to_square(3, 6)));
  REQUIRE_FALSE(output.outgoing_message.has_value());
}

TEST_CASE("temporal scheduler alternates boards after each event",
          "[agent][temporal][scheduler]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);
  TemporalState state = initial_temporal_state();
  TemporalTrace trace;

  REQUIRE(scheduler.next_actor(state) == to_player(0));
  PlayerId first = *scheduler.next_actor(state);
  Move move = generate_legal_moves(state.position, first).front();
  REQUIRE(scheduler.apply_event(state, first, move, trace));
  REQUIRE(state.preferred_board == 1);
  REQUIRE(scheduler.next_actor(state) == to_player(3));
  REQUIRE(state.events_remaining == 3);
}

TEST_CASE("temporal scheduler falls back from a stalled preferred board",
          "[agent][temporal][scheduler]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);
  TemporalState state = initial_temporal_state(1);
  state.position.boards[1].load_fen("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1");

  REQUIRE(is_stalemate(state.position, to_player(2)));
  REQUIRE(scheduler.next_actor(state) == to_player(0));
}

TEST_CASE("a transfer can make a stalled board schedulable",
          "[agent][temporal][scheduler][transfer]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);
  TemporalState state = initial_temporal_state();
  state.position.boards[0].load_fen("4k3/8/8/8/8/8/p7/R3K3 w - - 0 1");
  state.position.boards[1].load_fen("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1");
  TemporalTrace trace;

  REQUIRE(is_stalemate(state.position, to_player(2)));
  REQUIRE(scheduler.apply_event(state, to_player(0),
                                Move::normal(to_square(0, 0), to_square(0, 1)),
                                trace));
  REQUIRE(state.position.pockets[2].contains(PAWN));
  REQUIRE(scheduler.next_actor(state) == to_player(2));
}

TEST_CASE("tracked transfer consumption is partner-specific",
          "[agent][temporal][transfer]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);

  SECTION("partner consumes the transfer") {
    TemporalState state = initial_temporal_state(1);
    state.position.boards[1].load_fen("7k/5K2/6Q1/8/8/8/8/8 b - - 0 1");
    state.position.pockets[2].add(PAWN);
    state.transferred_piece_type = PAWN;
    state.transfer_still_available = true;
    TemporalTrace trace;
    Move drop = Move::drop(PAWN, to_square(0, 3));
    REQUIRE(scheduler.apply_event(state, to_player(2), drop, trace));
    REQUIRE(trace.partner_used_transfer);
    REQUIRE(trace.events[0].tracked_transfer_consumed);
  }

  SECTION("opponent use does not consume it") {
    TemporalState state = initial_temporal_state(1);
    state.position.boards[1].load_fen("7k/5K2/6Q1/8/8/8/8/8 w - - 0 1");
    state.position.pockets[3].add(PAWN);
    state.transferred_piece_type = PAWN;
    state.transfer_still_available = true;
    TemporalTrace trace;
    Move drop = Move::drop(PAWN, to_square(0, 3));
    REQUIRE(scheduler.apply_event(state, to_player(3), drop, trace));
    REQUIRE_FALSE(trace.partner_used_transfer);
    REQUIRE_FALSE(trace.events[0].tracked_transfer_consumed);
  }
}

TEST_CASE("temporal clocks charge both boards and increment only the actor",
          "[agent][temporal][clock]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);
  TemporalState state = initial_temporal_state();
  TemporalTrace trace;
  Move move = generate_legal_moves(state.position, to_player(0)).front();

  REQUIRE(scheduler.apply_event(state, to_player(0), move, trace));
  REQUIRE(state.remaining_ms[0] == 61'000);
  REQUIRE(state.remaining_ms[3] == 59'000);
  REQUIRE(state.remaining_ms[1] == 60'000);
  REQUIRE(state.remaining_ms[2] == 60'000);
  REQUIRE(trace.events[0].elapsed_ms == 1'000);
}

TEST_CASE("temporal clock produces deterministic flag and simultaneous draw",
          "[agent][temporal][clock][terminal]") {
  TemporalConfig config;
  TemporalScheduler scheduler(config);

  SECTION("one team flags") {
    TemporalState state = initial_temporal_state();
    state.remaining_ms[0] = 500;
    TemporalTrace trace;
    Move move = generate_legal_moves(state.position, to_player(0)).front();
    REQUIRE_FALSE(scheduler.apply_event(state, to_player(0), move, trace));
    REQUIRE(trace.game_result == GameResult::TEAM_B_WINS);
    REQUIRE(trace.event_count == 0);
  }

  SECTION("both teams flag") {
    TemporalState state = initial_temporal_state();
    state.remaining_ms[0] = 500;
    state.remaining_ms[3] = 500;
    TemporalTrace trace;
    Move move = generate_legal_moves(state.position, to_player(0)).front();
    REQUIRE_FALSE(scheduler.apply_event(state, to_player(0), move, trace));
    REQUIRE(trace.game_result == GameResult::DRAW);
  }
}