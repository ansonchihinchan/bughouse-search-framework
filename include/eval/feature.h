#pragma once

#include "eval/score.h"
#include "eval/types.h"

class ClassicalFeature {
public:
  virtual ~ClassicalFeature() = default;

  virtual EvalScore evaluate(const ClassicalContext &context) const = 0;

  virtual const std::string_view name() const = 0;
};

class BughouseFeature {
public:
  virtual ~BughouseFeature() = default;

  virtual EvalScore evaluate(const EvalContext &context) const = 0;

  virtual const std::string_view name() const = 0;
};