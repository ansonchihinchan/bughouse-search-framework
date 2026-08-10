#include "search/tree_search.h"
#include "game/movegen.h"
#include "search/searcher.h"
#include "search/see.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr int TT_MOVE_SCORE = 1000000;
constexpr int WINNING_CAPTURE_BASE = 800000;
constexpr int CHECK_THREAT_SCORE = 750000;
constexpr int KILLER1_SCORE = 700000;
constexpr int KILLER2_SCORE = 690000;
constexpr int COUNTER_MOVE_SCORE = 680000;
constexpr int MATING_THREAT_BONUS = 50000;
constexpr int LOSING_CAPTURE_BASE = -900000;

constexpr int MAX_MATE_PLY = 1024;
int score_to_tt(int score, int ply) {
  if (score >= INF_SCORE - MAX_MATE_PLY)
    return score + ply;
  if (score <= -INF_SCORE + MAX_MATE_PLY)
    return score - ply;
  return score;
}

int tt_to_score(int score, int ply) {
  if (score >= INF_SCORE - MAX_MATE_PLY)
    return score - ply;
  if (score <= -INF_SCORE + MAX_MATE_PLY)
    return score + ply;
  return score;
}

class RepetitionGuard {
public:
  RepetitionGuard(std::vector<RepetitionNode> &path, uint64_t key,
                  bool irreversible)
      : path_(path), pushed_(path.empty() || path.back().key != key) {
    if (!pushed_)
      return;

    int prev_reversible = path_.empty() ? 0 : path_.back().reversible_plies;
    int reversible_plies = irreversible ? 0 : prev_reversible + 1;
    int repetition = mark_repetition(path_, key, reversible_plies);

    path_.push_back(RepetitionNode{key, reversible_plies, repetition});
  }

  ~RepetitionGuard() {
    if (pushed_)
      path_.pop_back();
  }

  int repetition() const { return path_.back().repetition; }

  // Disabled copying
  RepetitionGuard(const RepetitionGuard &) = delete;
  RepetitionGuard &operator=(const RepetitionGuard &) = delete;

private:
  std::vector<RepetitionNode> &path_;
  bool pushed_;
};

inline bool is_repetition_draw(int repetition, int ply) {
  return repetition != 0 && repetition < ply;
}

class TreeSearcher : public Searcher {
public:
  TreeSearcher(TreeSearch &search, TranspositionTable &tt,
               const SearchParams &params, SearchStats &stats)
      : Searcher(search, tt, params), stats_(stats) {}

protected:
  void end_iteration(int depth, const SearchResult &best) override {
    (void)best;
    stats_.depth_reached = depth;
    stats_.nodes_by_depth.push_back(stats_.nodes);
  }

private:
  SearchStats &stats_;
};
} // namespace

bool TreeSearch::deadline_reached() const {
  return timer_.should_abandon(stats_, limits_, start_time_);
}

void TreeSearch::age_history() {
  ordinary_history_.age();
  attacking_drop_history_.age();
  defensive_drop_history_.age();
}

void TreeSearch::new_search(const SearchLimits &limits) {
  stats_ = SearchStats{};
  limits_ = limits;
  start_time_ = std::chrono::steady_clock::now();

  if (params_.tt_enabled)
    tt_.new_generation();

  killer_.clear();
  search_path_.clear();
}

void TreeSearch::end_search() {
  if (params_.age_history)
    age_history();
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
                              float volatility) const {
  if (!params_.lmr_enabled || depth < params_.lmr_min_depth ||
      move_index < params_.lmr_full_depth_moves)
    return 0;

  double base = 0.5 + std::log(static_cast<double>(depth)) *
                          std::log(static_cast<double>(move_index)) / 2.25;

  double reduction = base - params_.lmr_volatility_scale * volatility;

  return std::clamp(static_cast<int>(reduction), 0, depth - 1);
}

bool TreeSearch::is_reducible(const BughousePosition &position,
                              const SearchContext &context, Move move,
                              bool capture, bool in_check, bool check) const {
  if (move.is_drop() || capture || in_check || check)
    return false;

  const Board &board = position.boards[board_of(context.root_player)];
  if (creates_mating_threat(board, move, colour_of(context.root_player)))
    return false;

  return true;
}

int TreeSearch::futility_margin(int depth, float volatility) const {
  return params_.futility_base_margin +
         params_.futility_per_depth_margin * depth +
         static_cast<int>(params_.futility_volatility_scale * volatility);
}

void TreeSearch::order_moves(const BughousePosition &position,
                             const SearchContext &context,
                             const DetailedMove &prev,
                             std::vector<ScoredMove> &scored_moves,
                             const TTEntry *tt_entry, int ply) const {
  const Board &board = position.boards[board_of(context.root_player)];
  Colour mover_colour = colour_of(context.root_player);
  bool in_check = board.is_in_check();

  // Counter-move
  Move counter_move{};
  if (!prev.move.is_none() && !prev.piece.is_empty())
    counter_move = counter_move_.counter_move(prev.piece, prev.move.to);

  for (ScoredMove &scored_move : scored_moves) {
    const Move &move = scored_move.move;

    if (tt_entry && !tt_entry->best_move.is_none() &&
        move == tt_entry->best_move) {
      scored_move.score = TT_MOVE_SCORE;
      continue;
    }

    if (board.is_capture(move)) {
      if (params_.see_enabled) {
        int see_score = SEE::see_score(board, move);
        scored_move.score = (see_score >= 0) ? WINNING_CAPTURE_BASE + see_score
                                             : LOSING_CAPTURE_BASE + see_score;
      } else {
        scored_move.score = WINNING_CAPTURE_BASE;
      }
      continue;
    }

    if (move.is_drop() &&
        drop_gives_check(board, move.drop_pt, move.to, mover_colour)) {
      scored_move.score = WINNING_CAPTURE_BASE +
                          SEE::PIECE_VALUE[move.drop_pt] +
                          SEE::POCKET_BONUS[move.drop_pt];
      continue;
    }

    bool gives_check =
        !move.is_drop() && move_gives_check(board, move, mover_colour);

    if (gives_check || creates_mating_threat(board, move, mover_colour)) {
      scored_move.score =
          CHECK_THREAT_SCORE + (gives_check ? MATING_THREAT_BONUS : 0);
      continue;
    }

    int bonus = creates_mating_threat(board, move, mover_colour)
                    ? MATING_THREAT_BONUS
                    : 0;

    if (move == killer_.first(ply)) {
      scored_move.score = KILLER1_SCORE;
    } else if (move == killer_.second(ply)) {
      scored_move.score = KILLER2_SCORE;
    } else if (!counter_move.is_none() && move == counter_move) {
      scored_move.score = COUNTER_MOVE_SCORE;
    } else if (move.is_drop()) {
      Piece moved_piece = make_piece(mover_colour, move.drop_pt);
      const auto &table =
          in_check ? defensive_drop_history_ : attacking_drop_history_;
      int drop_safety = params_.see_enabled
                            ? SEE::see_drop_score(board, move.drop_pt, move.to)
                            : 0;
      scored_move.score = (drop_safety < 0)
                              ? LOSING_CAPTURE_BASE + drop_safety
                              : table.score(moved_piece, move.to) + bonus;
    } else {
      Piece moved_piece =
          move.is_drop()
              ? make_piece(colour_of(context.root_player), move.drop_pt)
              : board.piece_on(move.from);
      scored_move.score = ordinary_history_.score(moved_piece, move.to);
    }
  }

  // TODO: for each move select highest and search
  std::sort(scored_moves.begin(), scored_moves.end(),
            [](const ScoredMove &a, const ScoredMove &b) {
              return a.score > b.score;
            });
}

int TreeSearch::alpha_beta(BughousePosition &position,
                           const SearchContext &context,
                           const DetailedMove &prev, int depth, int alpha,
                           int beta, int ply, std::stop_token stop_token,
                           bool is_null_move) {
  stats_.nodes++;

  uint64_t key = position_hash(position) ^ context.comm_hash;

  bool irreversible = is_null_move || prev.move.is_drop() ||
                      prev.move.type == PROMOTE ||
                      prev.move.type == EN_PASSANT || prev.piece.type == PAWN;

  RepetitionGuard rep_guard(search_path_, key, irreversible);

  if (is_repetition_draw(rep_guard.repetition(), ply))
    return DRAW_SCORE;

  if (depth <= 0 || stop_token.stop_requested() || deadline_reached())
    return leaf_eval(position, context, alpha, beta, stop_token);

  int old_alpha = alpha;
  const TTEntry *tt_entry = nullptr;

  if (params_.tt_enabled) {
    tt_entry = tt_.probe(key);

    stats_.tt_stats.probes++;
    if (tt_entry)
      stats_.tt_stats.hits++;
  }

  if (tt_entry && tt_entry->depth >= depth) {
    stats_.tt_stats.cutoffs++;

    int tt_score = tt_to_score(tt_entry->score, ply);

    switch (tt_entry->bound) {
    case TTBound::EXACT:
      return tt_score;
    case TTBound::LOWER:
      alpha = std::max(alpha, tt_score);
      break;
    case TTBound::UPPER:
      beta = std::min(beta, tt_score);
      break;
    }

    if (alpha >= beta)
      return tt_score;
  }

  const Board &board = position.boards[board_of(context.root_player)];
  Colour side = colour_of(context.root_player);
  bool in_check = board.is_in_check();

  int null_move_reduction = params_.null_move_reduction;
  float volatility = evaluator_.volatility(position, context.root_player);
  null_move_reduction =
      std::max(1, params_.null_move_reduction -
                      static_cast<int>(std::lround(
                          volatility * (params_.null_move_reduction - 1))));

  // Null move pruning
  // TODO: verification search, adaptive reduction, zugzwang detection
  if (null_move_enabled() && depth >= params_.null_move_min_depth &&
      !in_check && board.has_non_pawn(side) && beta < INF_SCORE &&
      !(tt_entry && tt_entry->depth >= depth - null_move_reduction &&
        tt_entry->bound == TTBound::UPPER &&
        tt_to_score(tt_entry->score, ply) < beta)) {
    BoardUndo null_undo = make_null_move(position, context.root_player);

    int score = -alpha_beta(
        position,
        make_context(context.remaining, next_player(context.root_player),
                     context.comm_context, context.comm_hash),
        DetailedMove{}, depth - 1 - null_move_reduction, -beta, -beta + 1,
        ply + 1, stop_token, true);
    undo_null_move(position, context.root_player, null_undo);

    if (!stop_token.stop_requested() && !deadline_reached() && score >= beta) {
      stats_.null_move_cutoffs++;
      // Stores an empty move
      if (params_.tt_enabled)
        tt_.store(key, depth, score_to_tt(score, ply), Move{}, TTBound::LOWER);
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

  order_moves(position, context, prev, scored_moves, tt_entry, ply);

  // Futility pruning
  bool futility = params_.futility_enabled &&
                  depth <= params_.futility_max_depth && !in_check &&
                  beta - alpha == 1 && beta < INF_SCORE - 1;
  int futility_eval = 0;
  int margin = 0;
  if (futility) {
    margin = futility_margin(depth, volatility);
    futility_eval = evaluator_.evaluate(
        position, context.root_player, context.remaining, context.comm_context);
  }

  int best = -INF_SCORE;
  Move best_move;
  bool first_child = true;
  bool completed = true;
  int move_index = 0;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);

    if (capture && params_.see_enabled) {
      int see_score = SEE::see_score(board, move);
      if (see_score < params_.see_prune_threshold) {
        move_index++;
        continue;
      }
    }

    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    if (futility && move_index > 0 && !capture && !move.is_drop() &&
        !move_gives_check(board, move, colour_of(context.root_player)) &&
        !creates_mating_threat(board, move, colour_of(context.root_player)) &&
        futility_eval + margin <= alpha) {
      move_index++;
      continue;
    }

    if (tt_entry && !tt_entry->best_move.is_none() &&
        move == tt_entry->best_move)
      stats_.move_ordering_stats.tt_move_hits++;
    else if (move == killer_.first(ply) || move == killer_.second(ply))
      stats_.move_ordering_stats.killer_hits++;
    else if (capture)
      (SEE::see_score(board, move) >= 0
           ? stats_.move_ordering_stats.winning_captures
           : stats_.move_ordering_stats.losing_captures)++;
    else
      stats_.move_ordering_stats.history_ordered++;

    BughouseUndo undo = apply_move(position, context.root_player, move);

    bool check = position.boards[board_of(context.root_player)].is_in_check();
    SearchContext next =
        make_context(context.remaining, next_player(context.root_player),
                     context.comm_context, context.comm_hash);

    int score;

    DetailedMove child_prev{move, moved_piece};

    if (first_child) {
      score = search_first_move(position, next, child_prev, depth, alpha, beta,
                                ply, stop_token);
    } else {
      int reduction = 0;
      if (is_reducible(position, context, move, capture, in_check, check))
        reduction = lmr_reduction(depth, move_index, volatility);
      score = search_tail_move(position, next, child_prev, depth, alpha, beta,
                               ply, reduction, stop_token);
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

      if (!capture) {
        update_quiet_heuristics(move, depth, ply, moved_piece, in_check);
        counter_move_.update(prev, move);
      }

      break;
    }

    if (stop_token.stop_requested() || deadline_reached()) {
      completed = false;
      break;
    }

    first_child = false;
    move_index++;
  }

  if (completed && params_.tt_enabled) {
    TTBound bound = best <= old_alpha ? TTBound::UPPER
                    : best >= beta    ? TTBound::LOWER
                                      : TTBound::EXACT;
    tt_.store(key, depth, score_to_tt(best, ply), best_move, bound);
  }
  return best;
}

namespace {
std::vector<Move> extract_pv(BughousePosition position, PlayerId player,
                             const TranspositionTable &tt, uint64_t comm_hash,
                             int max_len) {
  std::vector<Move> pv;
  for (int i = 0; i < max_len; i++) {
    uint64_t key = position_hash(position) ^ comm_hash;
    const TTEntry *entry = tt.probe(key);
    if (!entry || entry->best_move.is_none())
      break;
    Move move = entry->best_move;
    if (!position.boards[board_of(player)].is_legal(move))
      break;
    apply_move(position, player, move);
    pv.push_back(move);
    player = next_player(player);
  }
  return pv;
}
} // namespace

SearchResult TreeSearch::search_root(const BughousePosition &position,
                                     const SearchContext &context, int depth,
                                     int alpha, int beta,
                                     std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  const Board &board = working.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();

  int old_alpha = alpha;

  uint64_t key = position_hash(working) ^ context.comm_hash;
  RepetitionGuard rep_guard(search_path_, key, false);

  auto moves = generate_legal_moves(working, context.root_player);

  // checkmate, stalemate
  if (moves.empty()) {
    result.score =
        in_check ? -INF_SCORE
                 : evaluator_.evaluate(working, context.root_player,
                                       context.remaining, context.comm_context);
    result.depth = depth;
    result.bound = TTBound::EXACT;
    return result;
  }

  if (board.halfMove >= HALFMOVE_LIMIT) {
    result.score = DRAW_SCORE;
    result.depth = depth;
    result.bound = TTBound::EXACT;
    return result;
  }

  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (Move move : moves)
    scored_moves.push_back(ScoredMove{move, 0});

  const TTEntry *tt_entry = params_.tt_enabled ? tt_.probe(key) : nullptr;

  order_moves(working, context, DetailedMove{}, scored_moves, tt_entry, 0);

  int best = -INF_SCORE;
  bool first_child = true;
  bool searched = false;
  bool completed = true;
  int move_index = 0;

  for (ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    bool capture = board.is_capture(move);
    Piece moved_piece =
        move.is_drop()
            ? make_piece(colour_of(context.root_player), move.drop_pt)
            : board.piece_on(move.from);

    BughouseUndo undo = apply_move(working, context.root_player, move);

    bool check = working.boards[board_of(context.root_player)].is_in_check();
    SearchContext next =
        make_context(context.remaining, next_player(context.root_player),
                     context.comm_context, context.comm_hash);

    int score;

    DetailedMove child_prev{move, moved_piece};

    if (first_child) {
      score = search_first_move(working, next, child_prev, depth, alpha, beta,
                                0, stop_token);
    } else {
      int reduction = 0;
      if (is_reducible(working, context, move, capture, in_check, check))
        reduction =
            lmr_reduction(depth, move_index,
                          evaluator_.volatility(working, context.root_player));
      score = search_tail_move(working, next, child_prev, depth, alpha, beta, 0,
                               reduction, stop_token);
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

      if (!capture) {
        update_quiet_heuristics(move, depth, 0, moved_piece, in_check);
        counter_move_.update(DetailedMove{}, move);
      }
      break;
    }

    if (stop_token.stop_requested() || deadline_reached()) {
      completed = false;
      break;
    }

    first_child = false;
    move_index++;
  }

  result.score =
      searched ? best
               : evaluator_.evaluate(working, context.root_player,
                                     context.remaining, context.comm_context);
  result.bound = result.score <= old_alpha ? TTBound::UPPER
                 : result.score >= beta    ? TTBound::LOWER
                                           : TTBound::EXACT;
  if (!result.best_move.is_none())
    result.pv = extract_pv(position, context.root_player, tt_, depth,
                           context.comm_hash);
  result.completed = completed;

  stats_.depth_reached = depth;
  stats_.nodes_by_depth.push_back(stats_.nodes);
  return result;
}

SearchResult TreeSearch::search(const BughousePosition &position,
                                const SearchContext &context,
                                const SearchLimits &limits,
                                std::stop_token stop_token) {
  new_search(limits);

  if (context.history)
    search_path_ = *context.history;

  // checkmate, stalemate
  if (generate_legal_moves(position, context.root_player).empty()) {
    stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    SearchResult best;
    best.stats = stats_;
    return best;
  }

  TreeSearcher searcher(*this, tt_, params_, stats_);
  SearchResult best = searcher.run(position, context, limits, stop_token);

  stats_.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - start_time_);
  best.stats = stats_;
  return best;
}