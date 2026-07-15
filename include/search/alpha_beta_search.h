#pragma once

#include "search/tree_search.h"
#include <vector>

class AlphaBetaSearch : public TreeSearch {
public:
  using TreeSearch::TreeSearch;
  const char *name() const override { return "alpha_beta"; }

protected:
  SearchResult search_root(const BughousePosition &position,
                           const SearchContext &context, int depth, int alpha,
                           int beta, std::stop_token stop_token) override;

  // PVS overrides
  virtual int alpha_beta(BughousePosition &position,
                         const SearchContext &context, int depth, int alpha,
                         int beta, int ply, std::stop_token stop_token);

  virtual void order_moves(const BughousePosition &position,
                           const SearchContext &context,
                           std::vector<ScoredMove> &moves,
                           const TTEntry *tt_entry, int ply) const;

  int quiescence(BughousePosition &position, const SearchContext &context,
                 int alpha, int beta, std::stop_token stop_token);

  int leaf_eval(BughousePosition &position, const SearchContext &context,
                int alpha, int beta, std::stop_token stop_token) override {
    if (evaluator_.is_noisy(position, context))
      return quiescence(position, context, alpha, beta, stop_token);
    else
      return evaluator_.evaluate(position, context);
  }

  static constexpr int MAX_PLY = 128;
  std::array<Move, MAX_PLY> killer1_{};
  std::array<Move, MAX_PLY> killer2_{};

  // TODO: history(prev_move, curr_move)
  std::array<std::array<int, SQUARE_NO>, PIECE_NO> ordinary_history_{};
  // drops played when not in check
  std::array<std::array<int, SQUARE_NO>, PIECE_NO> attacking_drop_history_{};
  // drops played when in check
  std::array<std::array<int, SQUARE_NO>, PIECE_NO> defensive_drop_history_{};

  void clear_killers() override;
  void age_history() override;
  void update_quiet_heuristics(Move move, int depth, int ply, Piece moved_piece,
                               bool in_check);

  // NullMoveSearch overrides
  virtual bool null_move_enabled() const { return false; }
  virtual int null_move_reduction() const { return 3; }
  virtual int null_move_min_depth() const { return 3; }

  // LMR
  static constexpr int LMR_MIN_DEPTH = 3;
  static constexpr int LMR_FULL_DEPTH_MOVES = 3;

  virtual int lmr_reduction(int depth, int move_index, bool is_volatile) const;
  bool is_reducible(const BughousePosition &position,
                    const SearchContext &context, Move move, bool capture,
                    bool in_check, bool check) const;

  static bool is_volatile(const BughousePosition &position);
};