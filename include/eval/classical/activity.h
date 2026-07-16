#pragma once

#include "eval/classical.h"

class ActivityEvaluator : public EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "activity"; }
};