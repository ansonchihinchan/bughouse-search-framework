#pragma once

#include "eval/classical.h"

class ActivityEvaluator : public ClassicalFeature {
public:
  EvalScore evaluate(const ClassicalContext &context) const override;

  const std::string_view name() const override { return "activity"; }
};