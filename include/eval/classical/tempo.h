#pragma once

#include "eval/classical.h"

class TempoEvaluator : EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "tempo"; }
};