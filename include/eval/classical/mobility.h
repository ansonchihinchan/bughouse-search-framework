#pragma once

#include "eval/classical.h"

class MobilityEvaluator : EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "mobility"; }
};