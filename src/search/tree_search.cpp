#include "search/tree_search.h"

bool TreeSearch::deadline_reached() const {
  if (limits_.max_nodes != 0 && stats_.nodes >= limits_.max_nodes)
    return true;
  if (limits_.move_time.count() != 0) {
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    if (elapsed >= limits_.move_time)
      return true;
  }
  return false;
}

SearchResult TreeSearch::search(const BughousePosition &position,
                                const SearchContext &context,
                                const SearchLimits &limits,
                                std::stop_token stop_token) {
  stats_ = SearchStats{};
  limits_ = limits;
  start_time_ = std::chrono::steady_clock::now();

  SearchResult best;
  int max_depth = limits.max_depth > 0 ? limits.max_depth : 128;

  for (int depth = 1; depth <= max_depth; depth++) {
    if (stop_token.stop_requested() || deadline_reached())
      break;

    SearchResult result = search_root(position, context, depth, stop_token);

    if (!result.best_move.is_none()) {
      best = result;
      stats_.depth_reached = depth;
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;
  }

  stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time_);

  best.stats = stats_;
  return best;
}