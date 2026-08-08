#pragma once

#include "eval/feature.h"

class CommunicationEvaluator : public BughouseFeature {
public:
  EvalScore evaluate(const EvalContext &context) const override;

  const std::string_view name() const override { return "communication"; }
};