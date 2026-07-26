#pragma once

#include "eval/evaluator.h"
#include "eval/feature.h"
#include "eval/types.h"

class ClassicalEvaluator : public Evaluator {

public:
  ClassicalEvaluator();

  int evaluate(const BughousePosition &position,
               const SearchContext &search_context) const override;

  bool is_noisy(const BughousePosition &position,
                const SearchContext &search_context) const override;

private:
  std::vector<std::unique_ptr<EvalFeature>> features_;
};