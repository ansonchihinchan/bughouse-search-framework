#pragma once

#include "search/pvs.h"

class NullMoveSearch : public PVS {
public:
  using PVS::PVS;
  const char *name() const override { return "null_move"; }

protected:
  int alpha_beta(BughousePosition &position, const SearchContext &context,
                 int depth, int alpha, int beta,
                 std::stop_token stop_token) override;

private:
  static constexpr int NULL_MOVE_REDUCTION = 3;
  static constexpr int NULL_MOVE_MIN_DEPTH = 3;
};