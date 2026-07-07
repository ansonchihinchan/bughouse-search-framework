#pragma once

#include "search/tree_search.h"
#include <vector>

class AlphaBetaSearch : public TreeSearch {
public:
  using TreeSearch::TreeSearch;
  const char *name() const override { return "alpha_beta"; }

protected:
  SearchResult search_root(const BughousePosition &position,
                           const SearchContext &context, int depth,
                           std::stop_token stop_token) override;

  // PVSearch/NullMoveSearch overrides
  virtual int alpha_beta(const BughousePosition &position,
                         const SearchContext &context, int depth, int alpha,
                         int beta, std::stop_token stop_token);

  virtual void order_moves(const BughousePosition &position,
                           const SearchContext &context,
                           std::vector<Move> &moves) const;
};