#pragma once

#include "game/clock.h"
#include "game/types.h"
#include "search/transposition_table.h"
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

inline constexpr int INF_SCORE = std::numeric_limits<int>::max() / 2;

inline constexpr int DRAW_SCORE = 0;

struct SearchLimits {
  int max_depth = 0;                      // 0 = unbounded
  uint64_t max_nodes = 0;                 // 0 = unbounded
  std::chrono::milliseconds move_time{0}; // 0 = unbounded
  bool infinite = false;                  // stop only via stop_token
};

struct MoveOrderingStats {
  uint64_t tt_move_hits = 0;
  uint64_t killer_hits = 0;
  uint64_t history_ordered = 0;
  uint64_t winning_captures = 0;
  uint64_t losing_captures = 0;
};

struct TTStats {
  uint64_t probes = 0;
  uint64_t hits = 0;
  uint64_t cutoffs = 0;
};

struct SearchStats {
  uint64_t nodes = 0;
  uint64_t null_move_cutoffs = 0;
  uint64_t beta_cutoffs = 0;
  uint64_t first_move_cutoffs = 0;
  int depth_reached = 0;
  std::chrono::milliseconds elapsed{0};
  std::vector<uint64_t> nodes_by_depth;

  // Benchmark
  TTStats tt_stats;
  MoveOrderingStats move_ordering_stats;
};

struct SearchContext {
  const std::array<int64_t, PLAYER_NO> remaining;
  PlayerId root_player;
  const std::vector<RepetitionNode> *history = nullptr;
};

inline SearchContext make_context(const BughouseClock &clock, PlayerId player) {
  std::array<int64_t, PLAYER_NO> remaining{};
  for (int i = 0; i < PLAYER_NO; i++)
    remaining[i] = clock.remaining(to_player(i));
  return SearchContext{remaining, player};
}

constexpr SearchContext
make_context(const std::array<int64_t, PLAYER_NO> &remaining, PlayerId player,
             const std::vector<RepetitionNode> *history = nullptr) {
  return SearchContext{remaining, player, history};
}

struct SearchResult {
  Move best_move;
  int score = 0;
  int depth = 0;
  TTBound bound = TTBound::EXACT;
  std::vector<Move> pv;
  SearchStats stats;
  bool completed = false;
};

struct SearchParams {
  bool tt_enabled = true;

  bool see_enabled = true;
  int see_prune_threshold = -50;
  int delta_margin = 200;

  bool quiescence_enabled = true;
  int quiescence_max_ply = 20;

  int aspiration_initial_window = 25;
  int aspiration_start_depth = 3;
  float aspiration_volatility_scale = 2.0f;

  bool lmr_enabled = true;
  int lmr_min_depth = 3;
  int lmr_full_depth_moves = 3;

  bool null_move_enabled = false;
  int null_move_reduction = 3;
  int null_move_min_depth = 3;

  bool futility_enabled = true;
  int futility_max_depth = 3;

  bool age_history = true;
};

struct ScoredMove {
  Move move;
  int score = 0;
};

struct DetailedMove {
  Move move;
  Piece piece;
};