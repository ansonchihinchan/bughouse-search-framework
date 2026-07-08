#include "search/pvs.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include <algorithm>

int PVS::alpha_beta(BughousePosition &position, const SearchContext &context,
                    int depth, int alpha, int beta,
                    std::stop_token stop_token) {
  stats_.nodes++;
  if (depth <= 0 || stop_token.stop_requested() || deadline_reached())
    return leaf_eval(position, context, alpha, beta, stop_token);

  int alpha_orig = alpha;
  uint64_t key = position_hash(position);
  const TTEntry *tt_entry = tt_.probe(key);
  if (tt_entry && tt_entry->depth >= depth) {
    if (tt_entry->bound == TTBound::EXACT)
      return tt_entry->score;
    if (tt_entry->bound == TTBound::LOWER)
      alpha = std::max(alpha, tt_entry->score);
    else if (tt_entry->bound == TTBound::UPPER)
      beta = std::min(beta, tt_entry->score);
    if (alpha >= beta)
      return tt_entry->score;
  }

  auto moves = generate_legal_moves(position, context.root_player);
  if (moves.empty())
    return leaf_eval(position, context, alpha, beta, stop_token);
  order_moves(position, context, moves);
  if (tt_entry && !tt_entry->best_move.is_none()) {
    auto it = std::find(moves.begin(), moves.end(), tt_entry->best_move);
    if (it != moves.end())
      std::rotate(moves.begin(), it, it + 1);
  }

  int best = -INF_SCORE;
  Move best_move;
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
      score = -alpha_beta(
          position,
          make_context(context.clock, next_player(context.root_player)),
          depth - 1, -alpha - 1, -alpha, stop_token);
      if (score > alpha && score < beta)
        score = -alpha_beta(
            position,
            make_context(context.clock, next_player(context.root_player)),
            depth - 1, -beta, -alpha, stop_token);
    }
    undo_move(position, context.root_player, move, undo);

    if (score > best) {
      best = score;
      best_move = move;
    }
    alpha = std::max(alpha, best);
    if (alpha >= beta) {
      stats_.beta_cutoffs++;
      break;
    }
    if (stop_token.stop_requested() || deadline_reached())
      break;
    first_child = false;
  }

  TTBound bound = best <= alpha_orig ? TTBound::UPPER
                  : best >= beta     ? TTBound::LOWER
                                     : TTBound::EXACT;
  tt_.store(key, depth, best, best_move, bound);
  return best;
}

SearchResult PVS::search_root(const BughousePosition &position,
                              const SearchContext &context, int depth,
                              int alpha, int beta, std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  auto moves = generate_legal_moves(working, context.root_player);
  order_moves(working, context, moves);

  uint64_t key = position_hash(working);
  if (const TTEntry *tt_entry = tt_.probe(key);
      tt_entry && !tt_entry->best_move.is_none()) {
    auto it = std::find(moves.begin(), moves.end(), tt_entry->best_move);
    if (it != moves.end())
      std::rotate(moves.begin(), it, it + 1);
  }

  int best = -INF_SCORE;
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
    if (alpha >= beta)
      break;
    if (stop_token.stop_requested() || deadline_reached())
      break;
    first_child = false;
  }
  result.score = best;
  return result;
}