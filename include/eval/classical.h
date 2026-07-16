#pragma once

#include "eval/evaluator.h"
#include "eval/feature.h"
#include "eval/types.h"

class ClassicalEvaluator : public Evaluator {
private:
  std::vector<std::unique_ptr<EvalFeature>> features;

public:
  int evaluate(const BughousePosition &position,
               const SearchContext &search_context) const override {
    EvalContext eval_context = to_context(position, search_context);

    EvalScore score = 0;

    for (const auto &feature : features)
      score += feature->evaluate(eval_context);

    return score.final(eval_context.material_info.phase);
  }
};