#pragma once

#include "search/alpha_beta_search.h"

class PVS : public AlphaBetaSearch {
public:
  using AlphaBetaSearch::AlphaBetaSearch;
  const std::string_view name() const override { return "pvs"; }

protected:
  int search_tail_move(BughousePosition &position, const SearchContext &next,
                       const DetailedMove &prev, int depth, int alpha, int beta,
                       int ply, int reduction, bool is_pv,
                       std::stop_token stop_token) override;
};