#pragma once

#include "eval/classical.h"

class KingSafetyEvaluator : public EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "king_safety"; }
};