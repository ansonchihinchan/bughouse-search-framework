#pragma once

#include "search/pvs.h"

class NullMoveSearch : public PVS {
public:
  using PVS::PVS;
  const std::string_view name() const override { return "null_move"; }

protected:
  bool null_move_enabled() const override { return true; }
};