#include "search/alpha_beta_search.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include <algorithm>

// TODO: possibly moved to a core layer
namespace {
constexpr int PIECE_VALUE[PIECE_TYPE_NO] = {0, 100, 320, 330, 550, 900, 20000};
constexpr int TT_MOVE_SCORE = 1000000;
constexpr int WINNING_CAPTURE_BASE = 800000;
constexpr int KILLER1_SCORE = 700000;
constexpr int KILLER2_SCORE = 690000;
constexpr int LOSING_CAPTURE_BASE = -900000;
} // namespace

void AlphaBetaSearch::clear_killers() {
  killer1_.fill(Move{});
  killer2_.fill(Move{});
}

void AlphaBetaSearch::age_history() {
  for (auto &row : history_)
    for (auto &value : row)
      value /= 2;
}

void AlphaBetaSearch::update_quiet_heuristics(Move move, int depth, int ply,
                                              Piece moved_piece) {
  if (move != killer1_[ply]) {
    killer2_[ply] = killer1_[ply];
    killer1_[ply] = move;
  }

  history_[moved_piece.index()][move.to] += depth * depth;
}

// TODO: new module for move ordering
// TODO: Threat-first ordering
void AlphaBetaSearch::order_moves(const BughousePosition &position,
                                  const SearchContext &context,
                                  std::vector<ScoredMove> &scored_moves,
                                  const TTEntry *tt_entry, int ply) const {
  const Board &board = position.boards[board_of(context.root_player)];

  // TODO: SEE
  for (ScoredMove &scored_move : scored_moves) {
    const Move &move = scored_move.move;

    if (tt_entry && !tt_entry->best_move.is_none() &&
        move == tt_entry->best_move) {
      scored_move.score = TT_MOVE_SCORE;
      continue;
    }

    if (board.is_capture(move)) {
      PieceType attacker =
          move.is_drop() ? move.drop_pt : board.piece_on(move.from).type;
      PieceType victim =
          (move.type == EN_PASSANT) ? PAWN : board.piece_on(move.to).type;
      int delta = PIECE_VALUE[victim] - PIECE_VALUE[attacker];
      scored_move.score = (delta >= 0) ? WINNING_CAPTURE_BASE + delta
                                       : LOSING_CAPTURE_BASE + delta;
      continue;
    }

    if (move == killer1_[ply]) {
      scored_move.score = KILLER1_SCORE;
    } else if (move == killer2_[ply]) {
      scored_move.score = KILLER2_SCORE;
    } else {
      Piece moved_piece =
          move.is_drop()
              ? make_piece(colour_of_player(context.root_player), move.drop_pt)
              : board.piece_on(move.from);
      scored_move.score = history_[moved_piece.index()][move.to];
    }

    // TODO: Counter move heuristic
  }

  // TODO: for each move select highest and search
  std::sort(scored_moves.begin(), scored_moves.end(),
            [](const ScoredMove &a, const ScoredMove &b) {
              return a.score > b.score;
            });
}

int AlphaBetaSearch::alpha_beta(BughousePosition &position,
                                const SearchContext &context, int depth,
                                int alpha, int beta, int ply,
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

  const Board &board = position.boards[board_of(context.root_player)];
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
  bool completed = true;
  int move_index = 0;

  for (ScoredMove &scored_move : scored_moves) {
    // TODO: LMR
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of_player(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(position, context.root_player, move);
    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, ply + 1, stop_token);
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

    if (stop_token.stop_requested() || deadline_reached()) {
      completed = false;
      break;
    }

    move_index++;
  }

  if (completed) {
    TTBound bound = best <= old_alpha ? TTBound::UPPER
                    : best >= beta    ? TTBound::LOWER
                                      : TTBound::EXACT;

    tt_.store(key, depth, best, best_move, bound);
  }
  return best;
}

SearchResult AlphaBetaSearch::search_root(const BughousePosition &position,
                                          const SearchContext &context,
                                          int depth, int alpha, int beta,
                                          std::stop_token stop_token) {
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

  order_moves(working, context, scored_moves, tt_entry, 0);

  int best = -INF_SCORE;
  bool searched = false;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of_player(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(working, context.root_player, move);
    int score = -alpha_beta(
        working, make_context(context.clock, next_player(context.root_player)),
        depth - 1, -beta, -alpha, 1, stop_token);
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
  }

  result.score = searched ? best : evaluator_.evaluate(working, context);
  return result;
}

int AlphaBetaSearch::quiescence(BughousePosition &position,
                                const SearchContext &context, int alpha,
                                int beta, std::stop_token stop_token) {
  // TODO: SEE filtering, delta pruning, capture ordering, check extensions
  stats_.nodes++;

  if (stop_token.stop_requested() || deadline_reached())
    return evaluator_.evaluate(position, context);

  const Board &board = position.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();

  int stand_pat = 0;

  if (!in_check) {
    stand_pat = evaluator_.evaluate(position, context);

    if (stand_pat >= beta)
      return stand_pat;

    alpha = std::max(alpha, stand_pat);
  }

  // TODO: Delta pruning
  std::vector<Move> moves = generate_legal_moves(position, context.root_player);

  if (in_check) {
    if (moves.empty())
      return -INF_SCORE + 1;
  } else {
    std::erase_if(moves,
                  [&board](const Move &m) { return !board.is_capture(m); });
  }

  // Cheap Most Valuable Victim ordering by default
  // TODO: MVV-LVA/SEE
  std::sort(moves.begin(), moves.end(), [&board](const Move &a, const Move &b) {
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