#include "search/alpha_beta_search.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include <algorithm>

void AlphaBetaSearch::order_moves(const BughousePosition &position,
                                  const SearchContext &context,
                                  std::vector<Move> &moves) const {
  const Board &board = position.boards[board_of(context.root_player)];

  // Cheap captures-first ordering by default
  std::stable_sort(moves.begin(), moves.end(),
                   [&board](const Move &a, const Move &b) {
                     return board.is_capture(a) && !board.is_capture(b);
                   });
}

int AlphaBetaSearch::alpha_beta(const BughousePosition &position,
                                const SearchContext &context, int depth,
                                int alpha, int beta,
                                std::stop_token stop_token) {
  stats_.nodes++;
  if (depth <= 0 || stop_token.stop_requested() || deadline_reached())
    return leaf_eval(position, context);

  auto moves = generate_legal_moves(position, context.root_player);
  if (moves.empty())
    return leaf_eval(position, context);
  order_moves(position, context, moves);

  BughousePosition working = position;
  int best = -INF_SCORE;
  for (Move move : moves) {
    BughouseUndo undo = apply_move(working, context.root_player, move);
    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, stop_token);
    undo_move(working, context.root_player, move, undo);

    best = std::max(best, score);
    alpha = std::max(alpha, best);
    if (alpha >= beta) {
      stats_.beta_cutoffs++;
      break;
    }
    if (stop_token.stop_requested() || deadline_reached())
      break;
  }
  return best;
}

SearchResult AlphaBetaSearch::search_root(const BughousePosition &position,
                                          const SearchContext &context,
                                          int depth,
                                          std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  auto moves = generate_legal_moves(working, context.root_player);
  order_moves(working, context, moves);

  int alpha = -INF_SCORE, beta = INF_SCORE, best = -INF_SCORE;
  for (Move move : moves) {
    BughouseUndo undo = apply_move(working, context.root_player, move);
    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, stop_token);
    undo_move(working, context.root_player, move, undo);

    if (score > best) {
      best = score;
      result.best_move = move;
    }
    alpha = std::max(alpha, best);
    if (stop_token.stop_requested() || deadline_reached())
      break;
  }
  result.score = best;
  return result;
}