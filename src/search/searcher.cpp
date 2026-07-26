#include "search/searcher.h"
#include "game/movegen.h"
#include <algorithm>
#include <chrono>

SearchResult Searcher::run(const BughousePosition &position,
                           const SearchContext &context,
                           const SearchLimits &limits,
                           std::stop_token stop_token) {
  search_.new_search(limits);

  SearchResult best;
  int max_depth = limits.max_depth > 0 ? limits.max_depth : 128;
  int prev_score = 0;

  for (int depth = 1; depth <= max_depth; depth++) {
    if (stop_token.stop_requested() || search_.deadline_reached())
      break;

    begin_iteration(depth);
    int alpha = -INF_SCORE, beta = INF_SCORE;
    int window = params_.aspiration_initial_window;
    if (depth >= params_.aspiration_start_depth) {
      alpha = std::max(-INF_SCORE, prev_score - window);
      beta = std::min(INF_SCORE, prev_score + window);
    }

    SearchResult result;

    for (;;) {
      result = search_.search_root(position, context, depth, alpha, beta,
                                   stop_token);

      if (stop_token.stop_requested() || search_.deadline_reached())
        break;

      if (result.score <= alpha) {
        alpha = std::max(-INF_SCORE, alpha - window);
        window *= 2;
      } else if (result.score >= beta) {
        beta = std::min(INF_SCORE, beta + window);
        window *= 2;
      } else {
        break;
      }
    }

    if (!result.best_move.is_none()) {
      best = result;
      prev_score = result.score;
      end_iteration(depth, best);
    } else {
      best.score = result.score;
      break;
    }

    if (stop_token.stop_requested() || search_.deadline_reached())
      break;
  }

  search_.end_search();

  return best;
}