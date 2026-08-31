#pragma once

#include "eval/bughouse.h"
#include "game/bughouse.h"
#include "search/types.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <stop_token>

inline constexpr size_t MAX_TEMPORAL_EVENTS = 5;
inline constexpr size_t MAX_TEMPORAL_CANDIDATES = 8;

struct TemporalConfig {
  int64_t simulated_move_cost_ms = 1000;
  int rollout_events = 4;
  int rollout_depth = 1;
  size_t max_candidates = MAX_TEMPORAL_CANDIDATES;
  int local_sacrifice_margin = 100;
  int temporal_gain_margin = 50;
  size_t rollout_tt_mb = 1;
};

struct TemporalState {
  BughousePosition position;
  std::array<int64_t, PLAYER_NO> remaining_ms{};
  int preferred_board = 0;
  int events_remaining = 0;
  PlayerId root_player = NO_PLAYER;
  int increment_ms = 0;
  PieceType transferred_piece_type = NO_PIECE_TYPE;
  bool transfer_still_available = false;
};

struct TemporalEvent {
  PlayerId actor = NO_PLAYER;
  Move move;
  int64_t elapsed_ms = 0;
  PieceType transferred_piece = NO_PIECE_TYPE;
  bool tracked_transfer_consumed = false;
};

enum class TemporalStopReason { Horizon, GameOver, NoLegalEvent, Interrupted };

struct TemporalTrace {
  std::array<TemporalEvent, MAX_TEMPORAL_EVENTS> events{};
  size_t event_count = 0;
  GameResult game_result = GameResult::ONGOING;
  TemporalStopReason stop_reason = TemporalStopReason::Horizon;
  bool partner_used_transfer = false;
  int team_score = 0;

  bool append(const TemporalEvent &event);
};

class TemporalScheduler {
public:
  explicit TemporalScheduler(const TemporalConfig &config) : config_(config) {}

  std::optional<PlayerId> next_actor(const TemporalState &state) const;

  // Applies one scheduled event, including deterministic concurrent clocks
  // Returns false when a flag occurs before the move can complete
  bool apply_event(TemporalState &state, PlayerId actor, Move move,
                   TemporalTrace &trace) const;

  GameResult position_result(const TemporalState &state) const;

private:
  GameResult advance_clocks(TemporalState &state) const;

  const TemporalConfig &config_;
};

struct TemporalDecisionStats {
  size_t candidate_count = 0;
  size_t sacrifice_candidates_considered = 0;
  size_t transfers_observed = 0;
  size_t transferred_pieces_used = 0;
  size_t rollout_events = 0;
  size_t local_search_calls = 0;
  std::chrono::milliseconds elapsed{0};
};

struct TemporalDecision {
  SearchResult result;
  TemporalTrace baseline_trace;
  TemporalTrace selected_trace;
  TemporalTrace last_candidate_trace;
  int baseline_local_score = 0;
  int selected_local_score = 0;
  int last_candidate_local_score = 0;
  bool selected_sacrifice = false;
  TemporalDecisionStats stats;
};

struct SacrificeEvidence {
  int baseline_local_score = 0;
  int candidate_local_score = 0;
  int baseline_team_score = 0;
  int candidate_team_score = 0;
  bool resource_transferred = false;
  bool causal_availability = false;
  bool partner_used_transfer = false;
};

bool qualifies_sacrifice(const SacrificeEvidence &evidence,
                         const TemporalConfig &config);

class TemporalCoordinator {
public:
  explicit TemporalCoordinator(TemporalConfig config = {});
  ~TemporalCoordinator();

  TemporalCoordinator(const TemporalCoordinator &) = delete;
  TemporalCoordinator &operator=(const TemporalCoordinator &) = delete;

  SearchResult
  select_move(const BughouseState &game, PlayerId root_player,
              const SearchResult &baseline, const SearchLimits &limits,
              std::stop_token stop_token,
              std::chrono::steady_clock::time_point decision_start);

  const TemporalDecision &last_decision() const { return last_decision_; }
  const TemporalConfig &config() const { return config_; }

private:
  struct Impl;
  TemporalConfig config_;
  std::unique_ptr<Impl> impl_;
  TemporalDecision last_decision_;
};