#pragma once

#include "search/types.h"
#include <chrono>

class Timer {
public:
  virtual ~Timer() = default;

  virtual bool
  should_abandon(const SearchStats &stats, const SearchLimits &limits,
                 std::chrono::steady_clock::time_point start) const = 0;

  virtual bool should_iterate(const SearchStats &stats,
                              const SearchLimits &limits, int next_depth,
                              std::chrono::steady_clock::time_point start) {
    (void)next_depth;
    return !should_abandon(stats, limits, start);
  }
};

class SimpleTimer : public Timer {
public:
  bool
  should_abandon(const SearchStats &stats, const SearchLimits &limits,
                 std::chrono::steady_clock::time_point start) const override {
    if (limits.max_nodes != 0 && stats.nodes >= limits.max_nodes)
      return true;
    if (limits.move_time.count() != 0) {
      auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start);
      if (elapsed >= limits.move_time)
        return true;
    }
    return false;
  }
};

inline const Timer &default_timer() {
  static SimpleTimer timer;
  return timer;
}