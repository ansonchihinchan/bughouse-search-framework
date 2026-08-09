#pragma once

#include "eval/evaluator.h"
#include "game/bughouse.h"
#include "search/types.h"
#include <atomic>
#include <stop_token>

// Abstract Search interface
class Search {
public:
  const Evaluator &evaluator() const { return evaluator_; }
  explicit Search(const Evaluator &evaluator) : evaluator_(evaluator) {}
  virtual ~Search() = default;

  // Disabled copying
  Search(const Search &) = delete;
  Search &operator=(const Search &) = delete;

  virtual SearchResult search(const BughousePosition &position,
                              const SearchContext &context,
                              const SearchLimits &limits,
                              std::stop_token stop_token) = 0;

  virtual const std::string_view name() const = 0;

protected:
  const Evaluator &evaluator_;
};