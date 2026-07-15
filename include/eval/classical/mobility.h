#pragma once

#include "eval/classical.h"
#include "eval/feature.h"

class MobilityEvaluator : EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "mobility"; }
};