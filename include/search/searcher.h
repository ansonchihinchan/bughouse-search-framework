#pragma once

#include "search/tree_search.h"

class Searcher {
public:
  Searcher(TreeSearch &search, TranspositionTable &tt,
           const SearchParams &params)
      : search_(search), tt_(tt), params_(params) {}

  virtual ~Searcher() = default;

  SearchResult run(const BughousePosition &position,
                   const SearchContext &context, const SearchLimits &limits,
                   std::stop_token stop_token);

protected:
  virtual void begin_iteration(int depth) { (void)depth; }
  virtual void end_iteration(int depth, const SearchResult &best) {
    (void)depth;
    (void)best;
  }

  TreeSearch &search_;
  TranspositionTable &tt_;
  const SearchParams &params_;
};