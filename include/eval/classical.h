#pragma once

#include "eval/feature.h"
#include "eval/types.h"
#include "game/board.h"

class ClassicalEvaluator {

public:
  ClassicalEvaluator();

  int evaluate(const Board &Board, Colour side) const;
  bool is_noisy(const Board &board) const;

private:
  std::vector<std::unique_ptr<ClassicalFeature>> features_;
};