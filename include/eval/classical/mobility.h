#pragma once

#include "eval/classical.h"

class MobilityEvaluator : public EvalFeature {
public:
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "mobility"; }
};