#pragma once

#include "eval/feature.h"

class PieceSquareEvaluator : public ClassicalFeature {
public:
  EvalScore evaluate(const ClassicalContext &context) const override;

  const std::string_view name() const override { return "piece_square"; }
};