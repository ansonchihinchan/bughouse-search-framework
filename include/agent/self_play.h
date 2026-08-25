#pragma once

#include "agent/experiment.h"
#include "agent/replay.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <stop_token>
#include <vector>

class Observer;

enum class GameClockMode { RealTime, Deterministic };

class DecisionTimer {
public:
  using time_point = std::chrono::steady_clock::time_point;
  virtual ~DecisionTimer() = default;
  virtual time_point now() const = 0;
};

class SteadyDecisionTimer final : public DecisionTimer {
public:
  time_point now() const override { return std::chrono::steady_clock::now(); }
};

inline constexpr size_t STRATEGY_TYPE_COUNT =
    static_cast<size_t>(StrategyType::Flag) + 1;

enum class SelfPlayTermination {
  GameOver,
  PlyLimit,
  NoLegalMove,
  InvalidAgentMove,
  Stopped
};

struct SelfPlayConfig {
  SearchLimits search_limits{};
  size_t max_plies = 512;
  int first_board = 0;
  GameClockMode clock_mode = GameClockMode::Deterministic;
  int64_t deterministic_move_time_ms = 1000;
  std::array<int64_t, PLAYER_NO> deterministic_player_move_time_ms{};
  const DecisionTimer *decision_timer = nullptr;
  Observer *observer = nullptr;
};

struct ScheduledBoardEvent {
  int board = -1;
  PlayerId player = NO_PLAYER;
  std::vector<Move> legal_moves;
};

class BoardEventScheduler {
public:
  explicit BoardEventScheduler(int tie_break_board = 0);

  std::optional<ScheduledBoardEvent>
  next_event(const BughousePosition &position) const;
  void complete_event(int board, int64_t elapsed_ms);
  int64_t ready_at_ms(int board) const;

private:
  int tie_break_board_ = 0;
  std::array<int64_t, BOARD_NO> ready_at_ms_{};
};

SearchLimits real_time_search_limits(const SearchLimits &configured,
                                     int64_t remaining_ms, int increment_ms);

struct SelfPlayResult {
  GameResult game_result = GameResult::ONGOING;
  SelfPlayTermination termination = SelfPlayTermination::PlyLimit;
  size_t plies = 0;
  size_t messages_sent = 0;
  size_t piece_requests_generated = 0;
  size_t strategy_requests_generated = 0;
  size_t requests_fulfilled = 0;
  size_t piece_transfers = 0;
  std::array<size_t, STRATEGY_TYPE_COUNT> messages_by_strategy{};
  std::array<size_t, PLAYER_NO> moves_by_player{};
  std::array<int64_t, PLAYER_NO> final_clocks_ms{};
  std::array<AgentType, PLAYER_NO> strategies{};
  AgentOutput::Metrics agent_metrics{};
  size_t accepted_sacrifice_transfers = 0;
  size_t accepted_sacrifice_transfers_used = 0;
  size_t successful_temporal_sacrifices = 0;
  size_t coordination_opportunities = 0;
  size_t coordinated_responses = 0;
  size_t synchrony_credit = 0;
  double synchrony_score = 0.0;
  size_t total_drops = 0;
  size_t wasted_drops = 0;
  double wasted_drop_rate = 0.0;
  GameReplay replay{};
  std::optional<PlayerId> invalid_player;
};

class SelfPlayRunner {
public:
  explicit SelfPlayRunner(AgentStrategyExperiment &experiment)
      : experiment_(experiment) {}

  SelfPlayResult run(BughouseState &game, const SelfPlayConfig &config,
                     std::stop_token stop_token = {});

private:
  AgentStrategyExperiment &experiment_;
};