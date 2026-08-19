#pragma once

#include "eval/feature.h"

class PocketEvaluator : public BughouseFeature {
public:
  explicit PocketEvaluator(bool include_partner_pockets = true)
      : include_partner_pockets_(include_partner_pockets) {}

  EvalScore evaluate(const EvalContext &context) const override;

  const std::string_view name() const override { return "pocket"; }

private:
  bool include_partner_pockets_;
};