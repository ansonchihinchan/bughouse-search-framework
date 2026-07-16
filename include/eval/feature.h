#pragma once

#include "eval/score.h"
#include "eval/types.h"

class EvalFeature {
public:
  virtual ~EvalFeature() = default;

  virtual EvalScore evaluate(const EvalContext &context) const = 0;

  virtual const char *name() const = 0;
};