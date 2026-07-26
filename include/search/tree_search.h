#pragma once

#include "game/attacks.h"
#include "search/counter_move.h"
#include "search/history.h"
#include "search/killer.h"
#include "search/search.h"
#include "search/timer.h"
#include "search/transposition_table.h"
#include "search/types.h"
#include <array>
#include <chrono>
#include <vector>

// Shared for every depth-based search.
// AlphaBeta, PVS and NullMove
class TreeSearch : public Search {
public:
  TreeSearch(const Evaluator &evaluator, TranspositionTable &tt,
             const SearchParams &params, const Timer &timer = default_timer())
      : Search(evaluator), tt_(tt), params_(params), timer_(timer) {}

  SearchResult search(const BughousePosition &position,
                      const SearchContext &context, const SearchLimits &limits,
                      std::stop_token stop_token) override final;

  SearchResult search_root(const BughousePosition &position,
                           const SearchContext &context, int depth, int alpha,
                           int beta, std::stop_token stop_token);

  int alpha_beta(BughousePosition &position, const SearchContext &context,
                 const DetailedMove &prev, int depth, int alpha, int beta,
                 int ply, std::stop_token stop_token);

  void new_search(const SearchLimits &limits);
  void end_search();

  bool deadline_reached() const;

protected:
  virtual int search_first_move(BughousePosition &position,
                                const SearchContext &next,
                                const DetailedMove &prev, int depth, int alpha,
                                int beta, int ply,
                                std::stop_token stop_token) = 0;

  virtual int search_tail_move(BughousePosition &position,
                               const SearchContext &next,
                               const DetailedMove &prev, int depth, int alpha,
                               int beta, int ply, int reduction,
                               std::stop_token stop_token) = 0;

  // Quiescence overrides only this to keep searching noisy moves
  virtual int leaf_eval(BughousePosition &position,
                        const SearchContext &context, int alpha, int beta,
                        std::stop_token stop_token) {
    (void)alpha;
    (void)beta;
    (void)stop_token;
    return evaluator_.evaluate(position, context);
  }

  virtual void order_moves(const BughousePosition &position,
                           const SearchContext &context,
                           const DetailedMove &prev,
                           std::vector<ScoredMove> &scored_moves,
                           const TTEntry *tt_entry, int ply) const;

  void age_history();
  void update_quiet_heuristics(Move move, int depth, int ply, Piece moved_piece,
                               bool in_check);

  virtual bool null_move_enabled() const { return params_.null_move_enabled; }

  virtual int lmr_reduction(int depth, int move_index, bool is_volatile) const;
  bool is_reducible(const BughousePosition &position,
                    const SearchContext &context, Move move, bool capture,
                    bool in_check, bool check) const;

  static bool is_volatile(const BughousePosition &position);

  static constexpr int HALFMOVE_LIMIT = 100;

  int futility_margin(int depth, float volatility) const;

  Killer killer_;
  History ordinary_history_;
  History attacking_drop_history_;
  History defensive_drop_history_;
  CounterMove counter_move_;

  SearchStats stats_;
  SearchLimits limits_;
  std::chrono::steady_clock::time_point start_time_;
  TranspositionTable &tt_;
  const SearchParams &params_;
  const Timer &timer_;
};

// TODO
// Cheap check for mating threat
inline bool creates_mating_threat(const Board &board, Move move, Colour mover) {
  Colour enemy = flip(mover);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  return std::abs(file_of(move.to) - file_of(ksq)) <= 1 &&
         std::abs(rank_of(move.to) - rank_of(ksq)) <= 1;
}

inline bool drop_gives_check(const Board &board, PieceType pt, Square to,
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

inline bool move_gives_check(const Board &board, Move move, Colour mover) {
  if (move.is_drop())
    return drop_gives_check(board, move.drop_pt, move.to, mover);

  Colour enemy = flip(mover);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  PieceType pt = board.piece_on(move.from).type;
  if (move.type == PROMOTE)
    pt = move.promote_pt;
  if (move.type == CASTLE)
    pt = ROOK;

  Square from_sq = move.from;
  Square to_sq = move.to;
  if (move.type == CASTLE) {
    bool kingside = move.to > move.from;
    from_sq = to_square(kingside ? 7 : 0, rank_of(move.from));
    to_sq = to_square(kingside ? 5 : 3, rank_of(move.from));
  }

  Bitboard occ = board.bitboard_all();
  occ &= ~(1ULL << from_sq);
  occ |= (1ULL << to_sq);

  switch (pt) {
  case KNIGHT:
    return (knight_attacks(to_sq) & king_bb) != 0;
  case BISHOP:
    return (bishop_attacks(to_sq, occ) & king_bb) != 0;
  case ROOK:
    return (rook_attacks(to_sq, occ) & king_bb) != 0;
  case QUEEN:
    return ((bishop_attacks(to_sq, occ) | rook_attacks(to_sq, occ)) &
            king_bb) != 0;
  case PAWN: {
    int file_diff = std::abs(file_of(ksq) - file_of(to_sq));
    int rank_diff = rank_of(ksq) - rank_of(to_sq);
    int expected_rank_diff = (mover == WHITE) ? 1 : -1;
    return file_diff == 1 && rank_diff == expected_rank_diff;
  }
  default:
    return false;
  }
}