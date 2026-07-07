#pragma once

#include "search/search.h"
#include <chrono>

// Shared for every depth-based search.
// AlphaBeta, PVS and NullMove

class TreeSearch : public Search {
public:
  using Search::Search;

  SearchResult search(const BughousePosition &position,
                      const SearchContext &context, const SearchLimits &limits,
                      std::stop_token stop_token) override final;

protected:
  virtual SearchResult search_root(const BughousePosition &position,
                                   const SearchContext &context, int depth,
                                   std::stop_token stop_token) = 0;

  // Quiescence subclass overrides only this to keep searching noisy moves
  virtual int leaf_eval(const BughousePosition &position,
                        const SearchContext &context) const {
    return evaluator_.evaluate(position, context);
  }

  bool deadline_reached() const;

  SearchStats stats_;
  SearchLimits limits_;
  std::chrono::steady_clock::time_point start_time_;
};