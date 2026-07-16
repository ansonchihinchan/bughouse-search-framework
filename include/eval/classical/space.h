#pragma once

#include "eval/classical.h"

class SpaceEvaluator : public EvalFeature {
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "space"; }
};