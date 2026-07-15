#pragma once

#include "eval/classical.h"
#include "eval/feature.h"

class MaterialEvaluator : EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "material"; }
};