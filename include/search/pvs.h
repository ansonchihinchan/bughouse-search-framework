#pragma once

#include "search/alpha_beta_search.h"

class PVS : public AlphaBetaSearch {
public:
  using AlphaBetaSearch::AlphaBetaSearch;
  const char *name() const override { return "pvs"; }

protected:
  int alpha_beta(BughousePosition &position, const SearchContext &context,
                 int depth, int alpha, int beta,
                 std::stop_token stop_token) override;
  SearchResult search_root(const BughousePosition &position,
                           const SearchContext &context, int depth, int alpha,
                           int beta, std::stop_token stop_token) override;
};