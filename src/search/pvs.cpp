#include "search/pvs.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include <algorithm>

int PVS::alpha_beta(BughousePosition &position, const SearchContext &context,
                    int depth, int alpha, int beta, int ply,
                    std::stop_token stop_token) {
  // TODO: Futility pruning
  stats_.nodes++;

  if (depth <= 0 || stop_token.stop_requested() || deadline_reached())
    return leaf_eval(position, context, alpha, beta, stop_token);

  int old_alpha = alpha;
  uint64_t key = position_hash(position);
  const TTEntry *tt_entry = tt_.probe(key);

  stats_.tt_probes++;
  if (tt_entry)
    stats_.tt_hits++;

  if (tt_entry && tt_entry->depth >= depth) {
    stats_.tt_cutoffs++;

    switch (tt_entry->bound) {
    case TTBound::EXACT:
      return tt_entry->score;
    case TTBound::LOWER:
      alpha = std::max(alpha, tt_entry->score);
      break;
    case TTBound::UPPER:
      beta = std::min(beta, tt_entry->score);
      break;
    }

    if (alpha >= beta)
      return tt_entry->score;
  }

  // Null move pruning
  const Board &board = position.boards[board_of(context.root_player)];
  Colour side = colour_of_player(context.root_player);
  bool in_check = board.is_in_check();

  // TODO: verification search, adaptive reduction, zugzwang detection
  if (null_move_enabled() && depth >= null_move_min_depth() &&
      !board.is_in_check() && board.has_non_pawn(side) && beta < INF_SCORE &&
      !(tt_entry && tt_entry->depth >= depth - null_move_reduction() &&
        tt_entry->bound == TTBound::UPPER && tt_entry->score < beta)) {
    BoardUndo null_undo = make_null_move(position, context.root_player);

    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1 - null_move_reduction(), -beta, -beta + 1, ply + 1,
        stop_token);
    undo_null_move(position, context.root_player, null_undo);

    if (!stop_token.stop_requested() && !deadline_reached() && score >= beta) {
      stats_.null_move_cutoffs++;
      // Stores an empty move
      tt_.store(key, depth, score, Move{}, TTBound::LOWER);
      return score;
    }
  }

  auto moves = generate_legal_moves(position, context.root_player);

  // checkmate, stalemate
  if (moves.empty())
    return leaf_eval(position, context, alpha, beta, stop_token);

  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (Move move : moves)
    scored_moves.push_back(ScoredMove{move, 0});
  order_moves(position, context, scored_moves, tt_entry, ply);

  int best = -INF_SCORE;
  Move best_move;
  bool first_child = true;
  int move_index = 0;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of_player(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(position, context.root_player, move);
    bool check = position.boards[board_of(context.root_player)].is_in_check();
    int reduction = 0;
    if (!first_child &&
        is_reducible(position, context, move, capture, in_check, check))
      reduction = lmr_reduction(depth, move_index);

    int score;
    SearchContext next =
        make_context(context.clock, next_player(context.root_player));
    if (first_child) {
      score = -alpha_beta(position, next, depth - 1, -beta, -alpha, ply + 1,
                          stop_token);
    } else {
      score = -alpha_beta(position, next, depth - 1 - reduction, -alpha - 1,
                          -alpha, ply + 1, stop_token);

      if (reduction > 0 && score > alpha)
        score = -alpha_beta(position, next, depth - 1, -alpha - 1, -alpha,
                            ply + 1, stop_token);

      if (score > alpha && score < beta)
        score = -alpha_beta(position, next, depth - 1, -beta, -alpha, ply + 1,
                            stop_token);
    }
    undo_move(position, context.root_player, move, undo);

    if (score > best) {
      best = score;
      best_move = move;
    }

    alpha = std::max(alpha, best);

    if (alpha >= beta) {
      stats_.beta_cutoffs++;
      if (move_index == 0)
        stats_.first_move_cutoffs++;

      if (!capture)
        update_quiet_heuristics(move, depth, ply, moved_piece);

      break;
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;

    first_child = false;
    move_index++;
  }

  TTBound bound = best <= old_alpha ? TTBound::UPPER
                  : best >= beta    ? TTBound::LOWER
                                    : TTBound::EXACT;
  tt_.store(key, depth, best, best_move, bound);
  return best;
}

SearchResult PVS::search_root(const BughousePosition &position,
                              const SearchContext &context, int depth,
                              int alpha, int beta, std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  const Board &board = working.boards[board_of(context.root_player)];

  auto moves = generate_legal_moves(working, context.root_player);
  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (Move move : moves)
    scored_moves.push_back(ScoredMove{move, 0});

  uint64_t key = position_hash(working);
  const TTEntry *tt_entry = tt_.probe(key);
  order_moves(position, context, scored_moves, tt_entry, 0);

  int best = -INF_SCORE;
  bool first_child = true;
  bool searched = false;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of_player(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(working, context.root_player, move);

    int score;
    SearchContext next =
        make_context(context.clock, next_player(context.root_player));
    if (first_child) {
      score =
          -alpha_beta(working, next, depth - 1, -beta, -alpha, 1, stop_token);
    } else {
      score = -alpha_beta(working, next, depth - 1, -alpha - 1, -alpha, 1,
                          stop_token);
      if (score > alpha && score < beta)
        score =
            -alpha_beta(working, next, depth - 1, -beta, -alpha, 1, stop_token);
    }
    undo_move(working, context.root_player, move, undo);
    searched = true;

    if (score > best) {
      best = score;
      result.best_move = move;
    }

    alpha = std::max(alpha, best);

    if (alpha >= beta) {
      if (!capture)
        update_quiet_heuristics(move, depth, 0, moved_piece);
      break;
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;
    first_child = false;
  }

  result.score = searched ? best : evaluator_.evaluate(working, context);
  return result;
}