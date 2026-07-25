#pragma once

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
  const BughouseClock &clock;
  PlayerId root_player;
};

constexpr SearchContext make_context(const BughouseClock &c, PlayerId p) {
  return SearchContext{c, p};
}

struct SearchResult {
  Move best_move;
  int score = 0;
  int depth = 0;
  TTBound bound = TTBound::EXACT;
  std::vector<Move> pv;
  SearchStats stats;
};

struct SearchParams {
  bool tt_enabled;

  bool see_enabled;
  int see_prune_threshold = -50;
  int delta_margin = 200;

  bool quiescence_enabled;
  int quiescence_max_ply = 20;

  int aspiration_initial_window = 25;
  int aspiration_start_depth = 3;

  bool lmr_enabled = true;
  int lmr_min_depth = 3;
  int lmr_full_depth_moves = 3;

  bool null_move_enabled = false;
  int null_move_reduction = 3;
  int null_move_min_depth = 3;

  bool age_history = true;
};

struct ScoredMove {
  Move move;
  int score = 0;
};