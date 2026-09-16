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
  bool budget_interrupted = false;

  for (int depth = 1; depth <= max_depth; depth++) {
    if (stop_token.stop_requested())
      break;
    if (search_.deadline_reached()) {
      budget_interrupted = true;
      break;
    }

    begin_iteration(depth);
    int alpha = -INF_SCORE, beta = INF_SCORE;
    int window = params_.aspiration_initial_window;
    if (depth >= params_.aspiration_start_depth) {
      float volatility =
          search_.evaluator().volatility(position, context.root_player);
      window = static_cast<int>(
          window * (1.0f + params_.aspiration_volatility_scale * volatility));
      alpha = std::max(-INF_SCORE, prev_score - window);
      beta = std::min(INF_SCORE, prev_score + window);
    }

    SearchResult result;

    for (;;) {
      result = search_.search_root(position, context, depth, alpha, beta,
                                   stop_token);

      if (stop_token.stop_requested() || search_.deadline_reached()) {
        budget_interrupted = !result.completed;
        break;
      }

      if (result.score <= alpha && alpha > -INF_SCORE) {
        alpha = std::max(-INF_SCORE, alpha - window);
        window = static_cast<int>(std::min<int64_t>(INF_SCORE, 2LL * window));
      } else if (result.score >= beta && beta < INF_SCORE) {
        beta = std::min(INF_SCORE, beta + window);
        window = static_cast<int>(std::min<int64_t>(INF_SCORE, 2LL * window));
      } else {
        break;
      }
    }

    if (!result.best_move.is_none()) {
      if (!result.completed)
        break;

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

  // Exhausted budget before depth one completes
  if (best.best_move.is_none() && budget_interrupted &&
      !stop_token.stop_requested()) {
    const auto legal = generate_legal_moves(position, context.root_player);
    if (!legal.empty()) {
      best = SearchResult{};
      best.best_move = legal.front();
      best.pv = {best.best_move};
      best.budget_fallback = true;
    }
  }

  search_.end_search();

  return best;
}