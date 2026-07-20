#pragma once

#include "communication/types.h"
#include <vector>

class RequestHistory {

public:
  void record(const PieceRequest &request);

  void clear();

private:
  std::vector<PieceRequest> history_;
};