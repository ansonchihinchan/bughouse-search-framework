#pragma once

#include "eval/classical.h"

class PawnEvaluator : EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "pawn"; }
};