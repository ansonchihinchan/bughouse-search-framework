#include "search/alpha_beta_search.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include <algorithm>

void AlphaBetaSearch::order_moves(const BughousePosition &position,
                                  const SearchContext &context,
                                  std::vector<Move> &moves) const {
  const Board &board = position.boards[board_of(context.root_player)];
  std::stable_sort(moves.begin(), moves.end(),
                   [&board](const Move &a, const Move &b) {
                     return board.is_capture(a) && !board.is_capture(b);
                   });
}

int AlphaBetaSearch::alpha_beta(BughousePosition &position,
                                const SearchContext &context, int depth,
                                int alpha, int beta,
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
  for (Move move : moves) {
    BughouseUndo undo = apply_move(position, context.root_player, move);
    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, stop_token);
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
  }

  TTBound bound = best <= alpha_orig ? TTBound::UPPER
                  : best >= beta     ? TTBound::LOWER
                                     : TTBound::EXACT;
  tt_.store(key, depth, best, best_move, bound);
  return best;
}

SearchResult AlphaBetaSearch::search_root(const BughousePosition &position,
                                          const SearchContext &context,
                                          int depth, int alpha, int beta,
                                          std::stop_token stop_token) {
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
  for (Move move : moves) {
    BughouseUndo undo = apply_move(working, context.root_player, move);
    int score = -alpha_beta(
        working, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, stop_token);
    undo_move(working, context.root_player, move, undo);

    if (score > best) {
      best = score;
      result.best_move = move;
    }
    alpha = std::max(alpha, best);
    if (alpha >= beta)
      break; // aspiration fail-high; TreeSearch re-searches with a wider window
    if (stop_token.stop_requested() || deadline_reached())
      break;
  }
  result.score = best;
  return result;
}

int AlphaBetaSearch::quiescence(BughousePosition &position,
                                const SearchContext &context, int alpha,
                                int beta, std::stop_token stop_token) {
  stats_.nodes++;
  if (stop_token.stop_requested() || deadline_reached())
    return evaluator_.evaluate(position, context);

  int stand_pat = evaluator_.evaluate(position, context);
  if (stand_pat >= beta)
    return stand_pat;
  alpha = std::max(alpha, stand_pat);

  const Board &board = position.boards[board_of(context.root_player)];
  auto moves = generate_legal_moves(position, context.root_player);
  std::erase_if(moves,
                [&board](const Move &m) { return !board.is_capture(m); });
  if (moves.empty())
    return stand_pat;

  // MVV-only ordering for now; a proper MVV-LVA/SEE pass is a good follow-up.
  std::stable_sort(
      moves.begin(), moves.end(), [&board](const Move &a, const Move &b) {
        return board.piece_on(a.to).type > board.piece_on(b.to).type;
      });

  for (Move move : moves) {
    BughouseUndo undo = apply_move(position, context.root_player, move);
    int score = -quiescence(
        position, make_context(context.clock, next_player(context.root_player)),
        -beta, -alpha, stop_token);
    undo_move(position, context.root_player, move, undo);

    if (score >= beta)
      return score;
    alpha = std::max(alpha, score);

    if (stop_token.stop_requested() || deadline_reached())
      break;
  }
  return alpha;
}