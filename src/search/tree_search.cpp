#include "search/tree_search.h"

namespace {
constexpr int ASPIRATION_INITIAL_WINDOW = 25;
constexpr int ASPIRATION_START_DEPTH = 3;
} // namespace

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
  tt_.new_search();

  SearchResult best;
  int max_depth = limits.max_depth > 0 ? limits.max_depth : 128;
  int prev_score = 0;

  for (int depth = 1; depth <= max_depth; depth++) {
    if (stop_token.stop_requested() || deadline_reached())
      break;

    int alpha = -INF_SCORE, beta = INF_SCORE;
    int window = ASPIRATION_INITIAL_WINDOW;
    if (depth >= ASPIRATION_START_DEPTH) {
      alpha = std::max(-INF_SCORE, prev_score - window);
      beta = std::min(INF_SCORE, prev_score + window);
    }

    SearchResult result;

    for (;;) {
      result = search_root(position, context, depth, alpha, beta, stop_token);

      if (stop_token.stop_requested() || deadline_reached())
        break;

      if (result.score <= alpha) {
        alpha = std::max(-INF_SCORE, alpha - window);
        window *= 2;
      } else if (result.score >= beta) {
        beta = std::min(INF_SCORE, beta + window);
        window *= 2;
      } else {
        // laned inside the window
        break;
      }
    }

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