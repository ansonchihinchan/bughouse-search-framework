#pragma once

#include "game/types.h"
#include <chrono>
#include <cstdint>
#include <limits>
#include <vector>

inline constexpr int INF_SCORE = std::numeric_limits<int>::max() / 2;

struct SearchLimits {
  int max_depth = 0;                      // 0 = unbounded
  uint64_t max_nodes = 0;                 // 0 = unbounded
  std::chrono::milliseconds move_time{0}; // 0 = unbounded
  bool infinite = false;                  // stop only via stop_token
};

struct SearchStats {
  uint64_t nodes = 0;
  uint64_t null_move_cutoffs = 0;
  uint64_t beta_cutoffs = 0;
  int depth_reached = 0;
  std::chrono::milliseconds elapsed{0};
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
  SearchStats stats;
};

struct ScoredMove {
  Move move;
  int score = 0;
};