#include "search/alpha_beta_search.h"
#include "game/attacks.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include "search/see.h"
#include <algorithm>
#include <cmath>

// TODO: possibly moved to a core layer
namespace {
constexpr int PIECE_VALUE[PIECE_TYPE_NO] = {0, 100, 320, 330, 550, 900, 20000};
constexpr int TT_MOVE_SCORE = 1000000;
constexpr int WINNING_CAPTURE_BASE = 800000;
constexpr int KILLER1_SCORE = 700000;
constexpr int KILLER2_SCORE = 690000;
constexpr int LOSING_CAPTURE_BASE = -900000;

constexpr int MATING_THREAT_BONUS = 50000;
constexpr int DELTA_MARGIN = 200;

// Cheap check for mating threat
bool creates_mating_threat(const Board &board, Move move, Colour mover) {
  Colour enemy = flip(mover);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  return std::abs(file_of(move.to) - file_of(ksq)) <= 1 &&
         std::abs(rank_of(move.to) - rank_of(ksq)) <= 1;
}

bool drop_gives_check(const Board &board, PieceType pt, Square to,
                      Colour colour) {
  Colour enemy = flip(colour);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard occ = board.bitboard_all();

  switch (pt) {
  case KNIGHT:
    return (knight_attacks(to) & king_bb) != 0;
  case BISHOP:
    return (bishop_attacks(to, occ) & king_bb) != 0;
  case ROOK:
    return (rook_attacks(to, occ) & king_bb) != 0;
  case QUEEN:
    return ((bishop_attacks(to, occ) | rook_attacks(to, occ)) & king_bb) != 0;
  case PAWN: {
    int file_diff = std::abs(file_of(ksq) - file_of(to));
    int rank_diff = rank_of(ksq) - rank_of(to);
    int expected_rank_diff = (colour == WHITE) ? 1 : -1;
    return file_diff == 1 && rank_diff == expected_rank_diff;
  }
  default:
    return false;
  }
}
} // namespace

void AlphaBetaSearch::clear_killers() {
  killer1_.fill(Move{});
  killer2_.fill(Move{});
}

void AlphaBetaSearch::age_history() {
  for (auto &row : ordinary_history_)
    for (auto &value : row)
      value /= 2;

  for (auto &row : attacking_drop_history_)
    for (auto &value : row)
      value /= 2;

  for (auto &row : defensive_drop_history_)
    for (auto &value : row)
      value /= 2;
}

bool AlphaBetaSearch::is_volatile(const BughousePosition &position) {
  // Cheap check if any player currently holds a queen or rook in their pocket
  for (const Pocket &pocket : position.pockets)
    if (pocket.contains(QUEEN) || pocket.contains(ROOK))
      return true;
  return false;
}

void AlphaBetaSearch::update_quiet_heuristics(Move move, int depth, int ply,
                                              Piece moved_piece,
                                              bool in_check) {
  if (move != killer1_[ply]) {
    killer2_[ply] = killer1_[ply];
    killer1_[ply] = move;
  }

  if (move.is_drop()) {
    auto &table = in_check ? defensive_drop_history_ : attacking_drop_history_;
    table[moved_piece.index()][move.to] += depth * depth;
  } else {
    ordinary_history_[moved_piece.index()][move.to] += depth * depth;
  }
}

int AlphaBetaSearch::lmr_reduction(int depth, int move_index,
                                   bool is_volatile) const {
  if (depth < LMR_MIN_DEPTH || move_index < LMR_FULL_DEPTH_MOVES)
    return 0;

  double r = 0.5 + std::log(static_cast<double>(depth)) *
                       std::log(static_cast<double>(move_index)) / 2.25;
  int reduction = static_cast<int>(r);
  return std::clamp(reduction, 1, depth - 1);

  int min_reduction = 1;
  if (is_volatile) {
    reduction = std::max(0, reduction - 1);
    min_reduction = 0;
  }

  return std::clamp(reduction, min_reduction, depth - 1);
}

bool AlphaBetaSearch::is_reducible(const BughousePosition &position,
                                   const SearchContext &context, Move move,
                                   bool capture, bool in_check,
                                   bool check) const {
  if (move.is_drop() || capture || in_check || check)
    return false;

  const Board &board = position.boards[board_of(context.root_player)];
  if (creates_mating_threat(board, move, colour_of_player(context.root_player)))
    return false;

  return true;
}

// TODO: new module for move ordering
// TODO: Threat-first ordering
void AlphaBetaSearch::order_moves(const BughousePosition &position,
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

    if (move == killer1_[ply]) {
      scored_move.score = KILLER1_SCORE;
    } else if (move == killer2_[ply]) {
      scored_move.score = KILLER2_SCORE;
    } else if (move.is_drop()) {
      Piece moved_pice = make_piece(mover_colour, move.drop_pt);
      const auto &table =
          in_check ? defensive_drop_history_ : attacking_drop_history_;
      scored_move.score = table[moved_pice.index()][move.to] + bonus;
    } else {
      Piece moved_piece =
          move.is_drop()
              ? make_piece(colour_of_player(context.root_player), move.drop_pt)
              : board.piece_on(move.from);
      scored_move.score = ordinary_history_[moved_piece.index()][move.to];
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
  bool in_check = board.is_in_check();
  bool is_volatile = this->is_volatile(position);
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
    int reduction = 0;
    if (is_reducible(position, context, move, capture, in_check, check))
      reduction = lmr_reduction(depth, move_index, is_volatile);

    int score;

    if (reduction > 0) {
      score = -alpha_beta(position, next, depth - 1 - reduction, -beta, -alpha,
                          ply + 1, stop_token);

      if (score > alpha)
        score = -alpha_beta(position, next, depth - 1, -beta, -alpha, ply + 1,
                            stop_token);
    } else {
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

SearchResult AlphaBetaSearch::search_root(const BughousePosition &position,
                                          const SearchContext &context,
                                          int depth, int alpha, int beta,
                                          std::stop_token stop_token) {
  SearchResult result;
  BughousePosition working = position;
  const Board &board = working.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();

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
    int reduction = 0;
    if (is_reducible(working, context, move, capture, in_check, check))
      reduction = lmr_reduction(depth, move_index, is_volatile(position));

    int score;
    if (reduction > 0) {
      score = -alpha_beta(working, next, depth - 1 - reduction, -beta, -alpha,
                          1, stop_token);
      if (score > alpha)
        score =
            -alpha_beta(working, next, depth - 1, -beta, -alpha, 1, stop_token);
    } else {
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
      if (move_index == 0)
        stats_.first_move_cutoffs++;

      if (!capture)
        update_quiet_heuristics(move, depth, 0, moved_piece, in_check);
      break;
    }

    if (stop_token.stop_requested() || deadline_reached())
      break;

    move_index++;
  }

  result.score = searched ? best : evaluator_.evaluate(working, context);
  return result;
}

int AlphaBetaSearch::quiescence(BughousePosition &position,
                                const SearchContext &context, int alpha,
                                int beta, std::stop_token stop_token) {
  // TODO: check extensions
  stats_.nodes++;

  if (stop_token.stop_requested() || deadline_reached())
    return evaluator_.evaluate(position, context);

  const Board &board = position.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();
  Colour mover_colour = colour_of_player(context.root_player);

  int stand_pat = 0;

  if (!in_check) {
    stand_pat = evaluator_.evaluate(position, context);

    if (stand_pat >= beta)
      return stand_pat;

    alpha = std::max(alpha, stand_pat);
  }

  std::vector<Move> moves = generate_legal_moves(position, context.root_player);

  if (in_check) {
    if (moves.empty())
      return -INF_SCORE + 1;
  } else {
    // Keep only captures and forcing drops
    std::erase_if(moves, [&](const Move &m) {
      return !((board.is_capture(m)) ||
               (m.is_drop() &&
                drop_gives_check(board, m.drop_pt, m.to, mover_colour)));
    });

    // SEE filtering
    std::erase_if(moves, [&board](const Move &m) {
      return board.is_capture(m) && SEE::see_score(board, m) < -50;
    });

    // Delta pruning
    std::erase_if(moves, [&](const Move &m) {
      if (m.is_drop())
        return false;

      int captured_value = (m.type == EN_PASSANT)
                               ? SEE::PIECE_VALUE[PAWN]
                               : SEE::PIECE_VALUE[board.piece_on(m.to).type];
      int promo_gain = (m.type == PROMOTE) ? SEE::PIECE_VALUE[m.promote_pt] -
                                                 SEE::PIECE_VALUE[PAWN]
                                           : 0;

      return stand_pat + captured_value + promo_gain + DELTA_MARGIN < alpha;
    });
  }

  std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
    int score_a =
        a.is_drop() ? SEE::PIECE_VALUE[a.drop_pt] + SEE::POCKET_BONUS[a.drop_pt]
                    : SEE::see_score(board, a);
    int score_b =
        b.is_drop() ? SEE::PIECE_VALUE[b.drop_pt] + SEE::POCKET_BONUS[b.drop_pt]
                    : SEE::see_score(board, b);
    return score_a > score_b;
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