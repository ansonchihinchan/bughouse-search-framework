#pragma once

#include "eval/classical.h"

class PieceSquareEvaluator : public EvalFeature {
public:
  EvalScore evaluate(const EvalContext &context) const override;

  const char *name() const override { return "piece_square"; }
};