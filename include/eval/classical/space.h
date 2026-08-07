#pragma once

#include "eval/feature.h"

class SpaceEvaluator : public ClassicalFeature {
public:
  EvalScore evaluate(const ClassicalContext &context) const override;

  const std::string_view name() const override { return "space"; }
};