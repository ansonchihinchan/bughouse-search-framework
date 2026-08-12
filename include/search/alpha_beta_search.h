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
                        const DetailedMove &prev, int depth, int alpha,
                        int beta, int ply, bool is_pv,
                        std::stop_token stop_token) override;

  int search_tail_move(BughousePosition &position, const SearchContext &next,
                       const DetailedMove &prev, int depth, int alpha, int beta,
                       int ply, int reduction, bool is_pv,
                       std::stop_token stop_token) override;

  int quiescence(BughousePosition &position, const SearchContext &context,
                 int alpha, int beta, int qply, std::stop_token stop_token);

  int leaf_eval(BughousePosition &position, const SearchContext &context,
                int alpha, int beta, std::stop_token stop_token) override {
    if (params_.quiescence_enabled &&
        evaluator_.is_noisy(position, context.root_player))
      return quiescence(position, context, alpha, beta, 0, stop_token);
    else
      return evaluator_.evaluate(position, context.root_player,
                                 context.remaining, context.comm_context);
  }
};