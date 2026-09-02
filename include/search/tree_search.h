#pragma once

#include "game/attacks.h"
#include "game/bughouse.h"
#include "search/counter_move.h"
#include "search/history.h"
#include "search/killer.h"
#include "search/search.h"
#include "search/timer.h"
#include "search/transposition_table.h"
#include "search/types.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
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
                 int ply, bool is_pv, std::stop_token stop_token,
                 bool is_null_move = false);

  void new_search(const SearchLimits &limits);
  void end_search();

  bool deadline_reached() const;

  int evaluate_position(const BughousePosition &position,
                        const SearchContext &context);

protected:
  virtual int search_first_move(BughousePosition &position,
                                const SearchContext &next,
                                const DetailedMove &prev, int depth, int alpha,
                                int beta, int ply, bool is_pv,
                                std::stop_token stop_token) = 0;

  virtual int search_tail_move(BughousePosition &position,
                               const SearchContext &next,
                               const DetailedMove &prev, int depth, int alpha,
                               int beta, int ply, int reduction, bool is_pv,
                               std::stop_token stop_token) = 0;

  // Quiescence overrides only this to keep searching noisy moves
  virtual int leaf_eval(BughousePosition &position,
                        const SearchContext &context, int alpha, int beta,
                        std::stop_token stop_token) {
    (void)alpha;
    (void)beta;
    (void)stop_token;
    return evaluate_position(position, context);
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

  virtual int lmr_reduction(int depth, int move_index, float volatility) const;
  bool is_reducible(bool capture, bool in_check, bool check,
                    bool mating_threat) const;

  int futility_margin(int depth, float volatility) const;

  Killer killer_;
  History ordinary_history_;
  History attacking_drop_history_;
  History defensive_drop_history_;
  CounterMove counter_move_;

  std::vector<RepetitionNode> search_path_;

  SearchStats stats_;
  SearchLimits limits_;
  std::chrono::steady_clock::time_point start_time_;
  TranspositionTable &tt_;
  const SearchParams &params_;
  const Timer &timer_;

  struct EvalCacheEntry {
    uint64_t key = 0;
    int score = 0;
    bool valid = false;
  };
  static constexpr size_t EVAL_CACHE_SIZE = 1U << 16;
  std::vector<EvalCacheEntry> eval_cache_{EVAL_CACHE_SIZE};
};

// TODO
// Cheap check for mating threat, at least half of king's escape squares covered
inline bool creates_mating_threat(const Board &board, Move move, Colour mover) {
  Colour enemy = flip(mover);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;

  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard king_zone = king_attacks(ksq) | king_bb;

  PieceType pt;
  Square to_sq = move.to;

  if (move.is_drop()) {
    pt = move.drop_pt;
  } else {
    pt = board.piece_on(move.from).type;
    if (move.type == PROMOTE)
      pt = move.promote_pt;
    if (move.type == CASTLE) {
      bool kingside = move.to > move.from;
      to_sq = to_square(kingside ? 5 : 3, rank_of(move.from));
      pt = ROOK;
    }
  }

  Bitboard occ = board.bitboard_all();
  if (!move.is_drop())
    occ &= ~(1ULL << move.from);
  occ |= (1ULL << to_sq);

  Bitboard reach;
  switch (pt) {
  case KNIGHT:
    reach = knight_attacks(to_sq);
    break;
  case BISHOP:
    reach = bishop_attacks(to_sq, occ);
    break;
  case ROOK:
    reach = rook_attacks(to_sq, occ);
    break;
  case QUEEN:
    reach = bishop_attacks(to_sq, occ) | rook_attacks(to_sq, occ);
    break;
  case KING:
    reach = king_attacks(to_sq);
    break;
  default:
    reach = 0;
    break;
  }

  if (!(reach & king_zone))
    return false;

  Bitboard enemy_pieces = board.bitboard_colour(enemy);
  Bitboard flight_squares =
      king_attacks(ksq) & ~enemy_pieces & ~(1ULL << to_sq);

  if (!flight_squares)
    return false;

  Bitboard remaining = flight_squares & ~reach;

  return std::popcount(remaining) <= std::popcount(flight_squares) / 2;
}