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

  // PVSearch/NullMoveSearch overrides
  virtual int alpha_beta(BughousePosition &position,
                         const SearchContext &context, int depth, int alpha,
                         int beta, std::stop_token stop_token);

  virtual void order_moves(const BughousePosition &position,
                           const SearchContext &context,
                           std::vector<Move> &moves) const;

  int quiescence(BughousePosition &position, const SearchContext &context,
                 int alpha, int beta, std::stop_token stop_token);

  int leaf_eval(BughousePosition &position, const SearchContext &context,
                int alpha, int beta, std::stop_token stop_token) override {
    return quiescence(position, context, alpha, beta, stop_token);
  }
};