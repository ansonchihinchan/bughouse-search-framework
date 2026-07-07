#include "search/pvs.h"
#include "game/movegen.h"
#include <algorithm>

int PVSearch::alpha_beta(BughousePosition &position,
                         const SearchContext &context, int depth, int alpha,
                         int beta, std::stop_token stop_token) {
  stats_.nodes++;
  if (depth <= 0 || stop_token.stop_requested() || deadline_reached())
    return leaf_eval(position, context);

  auto moves = generate_legal_moves(position, context.root_player);
  if (moves.empty())
    return leaf_eval(position, context);
  order_moves(position, context, moves);

  int best = -INF_SCORE;
  bool first_child = true;

  for (Move move : moves) {
    BughouseUndo undo = apply_move(position, context.root_player, move);

    int score;
    if (first_child) {
      score = -alpha_beta(
          position,
          make_context(context.clock, next_player(context.root_player)),
          depth - 1, -beta, -alpha, stop_token);
    } else {
      // scout
      score = -alpha_beta(
          position,
          make_context(context.clock, next_player(context.root_player)),
          depth - 1, -alpha - 1, -alpha, stop_token);

      // re-search
      if (score > alpha && score < beta)
        score = -alpha_beta(
            position,
            make_context(context.clock, next_player(context.root_player)),
            depth - 1, -beta, -alpha, stop_token);
    }
    undo_move(position, context.root_player, move, undo);

    best = std::max(best, score);
    alpha = std::max(alpha, best);
    if (alpha >= beta) {
      stats_.beta_cutoffs++;
      break;
    }
    if (stop_token.stop_requested() || deadline_reached())
      break;
    first_child = false;
  }
  return best;
}

SearchResult PVSearch::search_root(const BughousePosition &position,
                                   const SearchContext &context, int depth,
                                   std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  auto moves = generate_legal_moves(working, context.root_player);
  order_moves(working, context, moves);

  int alpha = -INF_SCORE, beta = INF_SCORE, best = -INF_SCORE;
  bool first_child = true;

  for (Move move : moves) {
    BughouseUndo undo = apply_move(working, context.root_player, move);

    int score;
    if (first_child) {
      score = -alpha_beta(
          working,
          make_context(context.clock, next_player(context.root_player)),
          depth - 1, -beta, -alpha, stop_token);
    } else {
      score = -alpha_beta(
          working,
          make_context(context.clock, next_player(context.root_player)),
          depth - 1, -alpha - 1, -alpha, stop_token);
      if (score > alpha && score < beta)
        score = -alpha_beta(
            working,
            make_context(context.clock, next_player(context.root_player)),
            depth - 1, -beta, -alpha, stop_token);
    }
    undo_move(working, context.root_player, move, undo);

    if (score > best) {
      best = score;
      result.best_move = move;
    }
    alpha = std::max(alpha, best);
    if (stop_token.stop_requested() || deadline_reached())
      break;
    first_child = false;
  }
  result.score = best;
  return result;
}