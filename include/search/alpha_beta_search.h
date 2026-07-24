#pragma once

#include "search/history.h"
#include "search/killer.h"
#include "search/tree_search.h"
#include <vector>

class AlphaBetaSearch : public TreeSearch {
public:
  using TreeSearch::TreeSearch;
  const std::string_view name() const override { return "alpha_beta"; }

protected:
  int search_first_move(BughousePosition &position, const SearchContext &next,
                        int depth, int alpha, int beta, int ply,
                        std::stop_token stop_token) override;

  int search_tail_move(BughousePosition &position, const SearchContext &next,
                       int depth, int alpha, int beta, int ply, int reduction,
                       std::stop_token stop_token) override;

  int quiescence(BughousePosition &position, const SearchContext &context,
                 int alpha, int beta, int qply, std::stop_token stop_token);

  int leaf_eval(BughousePosition &position, const SearchContext &context,
                int alpha, int beta, std::stop_token stop_token) override {
    if (evaluator_.is_noisy(position, context))
      return quiescence(position, context, alpha, beta, 0, stop_token);
    else
      return evaluator_.evaluate(position, context);
  }
};