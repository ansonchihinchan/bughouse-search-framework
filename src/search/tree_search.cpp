#include "search/tree_search.h"
#include "game/movegen.h"
#include "search/see.h"
#include <cmath>

namespace {
constexpr int ASPIRATION_INITIAL_WINDOW = 25;
constexpr int ASPIRATION_START_DEPTH = 3;

constexpr int TT_MOVE_SCORE = 1000000;
constexpr int WINNING_CAPTURE_BASE = 800000;
constexpr int KILLER1_SCORE = 700000;
constexpr int KILLER2_SCORE = 690000;
constexpr int LOSING_CAPTURE_BASE = -900000;
constexpr int MATING_THREAT_BONUS = 50000;
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

void TreeSearch::age_history() {
  ordinary_history_.age();
  attacking_drop_history_.age();
  defensive_drop_history_.age();
}

bool TreeSearch::is_volatile(const BughousePosition &position) {
  // Cheap check if any player currently holds a queen or rook in their pocket
  for (const Pocket &pocket : position.pockets)
    if (pocket.contains(QUEEN) || pocket.contains(ROOK))
      return true;
  return false;
}

void TreeSearch::update_quiet_heuristics(Move move, int depth, int ply,
                                         Piece moved_piece, bool in_check) {
  killer_.update(ply, move);

  if (move.is_drop()) {
    History &table =
        in_check ? defensive_drop_history_ : attacking_drop_history_;
    table.add(moved_piece, move.to, depth);
  } else {
    ordinary_history_.add(moved_piece, move.to, depth);
  }
}

int TreeSearch::lmr_reduction(int depth, int move_index,
                              bool is_volatile) const {
  if (depth < LMR_MIN_DEPTH || move_index < LMR_FULL_DEPTH_MOVES)
    return 0;

  double r = 0.5 + std::log(static_cast<double>(depth)) *
                       std::log(static_cast<double>(move_index)) / 2.25;
  int reduction = static_cast<int>(r);
  int min_reduction = 1;
  if (is_volatile) {
    reduction = std::max(0, reduction - 1);
    min_reduction = 0;
  }

  return std::clamp(reduction, min_reduction, depth - 1);
}

bool TreeSearch::is_reducible(const BughousePosition &position,
                              const SearchContext &context, Move move,
                              bool capture, bool in_check, bool check) const {
  if (move.is_drop() || capture || in_check || check)
    return false;

  const Board &board = position.boards[board_of(context.root_player)];
  if (creates_mating_threat(board, move, colour_of_player(context.root_player)))
    return false;

  return true;
}

// TODO: new module for move ordering
// TODO: Threat-first ordering
void TreeSearch::order_moves(const BughousePosition &position,
                             const SearchContext &context,
                             std::vector<ScoredMove> &scored_moves,
                             const TTEntry *tt_entry, int ply) const {
  const Board &board = position.boards[board_of(context.root_player)];
  Colour mover_colour = colour_of_player(context.root_player);
  bool in_check = board.is_in_check();

  for (ScoredMove &scored_move : scored_moves) {
    const Move &move = scored_move.move;

    if (tt_entry && !tt_entry->best_move.is_none() &&
        move == tt_entry->best_move) {
      scored_move.score = TT_MOVE_SCORE;
      continue;
    }

    if (board.is_capture(move)) {
      int see_score = SEE::see_score(board, move);
      scored_move.score = (see_score >= 0) ? WINNING_CAPTURE_BASE + see_score
                                           : LOSING_CAPTURE_BASE + see_score;
      continue;
    }

    if (move.is_drop() &&
        drop_gives_check(board, move.drop_pt, move.to, mover_colour)) {
      scored_move.score = WINNING_CAPTURE_BASE +
                          SEE::PIECE_VALUE[move.drop_pt] +
                          SEE::POCKET_BONUS[move.drop_pt];
      continue;
    }

    int bonus = creates_mating_threat(board, move, mover_colour)
                    ? MATING_THREAT_BONUS
                    : 0;

    if (move == killer_.first(ply)) {
      scored_move.score = KILLER1_SCORE;
    } else if (move == killer_.second(ply)) {
      scored_move.score = KILLER2_SCORE;
    } else if (move.is_drop()) {
      Piece moved_piece = make_piece(mover_colour, move.drop_pt);
      const auto &table =
          in_check ? defensive_drop_history_ : attacking_drop_history_;
      scored_move.score = table.score(moved_piece, move.to) + bonus;
    } else {
      Piece moved_piece =
          move.is_drop()
              ? make_piece(colour_of_player(context.root_player), move.drop_pt)
              : board.piece_on(move.from);
      scored_move.score = ordinary_history_.score(moved_piece, move.to);
    }

    // TODO: Counter move heuristic
  }

  // TODO: for each move select highest and search
  std::sort(scored_moves.begin(), scored_moves.end(),
            [](const ScoredMove &a, const ScoredMove &b) {
              return a.score > b.score;
            });
}

int TreeSearch::alpha_beta(BughousePosition &position,
                           const SearchContext &context, int depth, int alpha,
                           int beta, int ply, std::stop_token stop_token) {
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
  Colour side = colour_of_player(context.root_player);
  bool in_check = board.is_in_check();
  bool is_volatile = this->is_volatile(position);

  // Null move pruning
  // TODO: verification search, adaptive reduction, zugzwang detection
  if (null_move_enabled() && depth >= null_move_min_depth() && !in_check &&
      board.has_non_pawn(side) && beta < INF_SCORE &&
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
    return in_check ? -INF_SCORE + ply
                    : leaf_eval(position, context, alpha, beta, stop_token);

  if (board.halfMove >= HALFMOVE_LIMIT)
    return DRAW_SCORE;

  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (Move move : moves)
    scored_moves.push_back(ScoredMove{move, 0});

  order_moves(position, context, scored_moves, tt_entry, ply);

  int best = -INF_SCORE;
  Move best_move;
  bool first_child = true;
  bool completed = true;
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
    SearchContext next =
        make_context(context.clock, next_player(context.root_player));

    int score;
    if (first_child) {
      score = search_first_move(position, next, depth, alpha, beta, ply,
                                stop_token);
    } else {
      int reduction = 0;
      if (is_reducible(position, context, move, capture, in_check, check))
        reduction = lmr_reduction(depth, move_index, is_volatile);
      score = search_tail_move(position, next, depth, alpha, beta, ply,
                               reduction, stop_token);
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
        update_quiet_heuristics(move, depth, ply, moved_piece, in_check);

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

SearchResult TreeSearch::search_root(const BughousePosition &position,
                                     const SearchContext &context, int depth,
                                     int alpha, int beta,
                                     std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  const Board &board = working.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();

  auto moves = generate_legal_moves(working, context.root_player);

  // checkmate, stalemate
  if (moves.empty()) {
    result.score =
        in_check ? -INF_SCORE : evaluator_.evaluate(working, context);
    return result;
  }

  if (board.halfMove >= HALFMOVE_LIMIT) {
    result.score = DRAW_SCORE;
    return result;
  }

  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (Move move : moves)
    scored_moves.push_back(ScoredMove{move, 0});

  uint64_t key = position_hash(working);
  const TTEntry *tt_entry = tt_.probe(key);

  order_moves(working, context, scored_moves, tt_entry, 0);

  int best = -INF_SCORE;
  bool first_child = true;
  bool searched = false;
  int move_index = 0;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of_player(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(working, context.root_player, move);

    bool check = working.boards[board_of(context.root_player)].is_in_check();
    SearchContext next =
        make_context(context.clock, next_player(context.root_player));

    int score;
    if (first_child) {
      score =
          search_first_move(working, next, depth, alpha, beta, 0, stop_token);
    } else {
      int reduction = 0;
      if (is_reducible(working, context, move, capture, in_check, check))
        reduction = lmr_reduction(depth, move_index, is_volatile(working));
      score = search_tail_move(working, next, depth, alpha, beta, 0, reduction,
                               stop_token);
    }

    undo_move(working, context.root_player, move, undo);
    searched = true;

    if (score > best) {
      best = score;
      result.best_move = move;
    }

    alpha = std::max(alpha, best);

    if (alpha >= beta) {
      if (move_index == 0)
        stats_.first_move_cutoffs++;

      if (!capture)
        update_quiet_heuristics(move, depth, 0, moved_piece, in_check);
      break;
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;

    first_child = false;
    move_index++;
  }

  result.score = searched ? best : evaluator_.evaluate(working, context);
  return result;
}

SearchResult TreeSearch::search(const BughousePosition &position,
                                const SearchContext &context,
                                const SearchLimits &limits,
                                std::stop_token stop_token) {
  stats_ = SearchStats{};
  limits_ = limits;
  start_time_ = std::chrono::steady_clock::now();
  tt_.new_search();
  killer_.clear();

  SearchResult best;
  int max_depth = limits.max_depth > 0 ? limits.max_depth : 128;
  int prev_score = 0;

  for (int depth = 1; depth <= max_depth; depth++) {
    if (stop_token.stop_requested() || deadline_reached())
      break;

    int alpha = -INF_SCORE, beta = INF_SCORE;
    // TODO: adapt initial window
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
        break;
      }
    }

    if (!result.best_move.is_none()) {
      best = result;
      stats_.depth_reached = depth;
      stats_.nodes_by_depth.push_back(stats_.nodes);
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;
  }

  age_history();

  stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time_);

  best.stats = stats_;
  return best;
}