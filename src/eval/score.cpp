#include "eval/score.h"

constexpr int MAX_PHASE = 24;

EvalScore &EvalScore::operator+=(const EvalScore &rhs) {
  mid_game_ += rhs.mid_game_;
  end_game_ += rhs.end_game_;
  return *this;
}

EvalScore &EvalScore::operator-=(const EvalScore &rhs) {
  mid_game_ -= rhs.mid_game_;
  end_game_ -= rhs.end_game_;
  return *this;
}

EvalScore operator+(EvalScore lhs, const EvalScore &rhs) {
  lhs += rhs;
  return lhs;
}

EvalScore operator-(EvalScore lhs, const EvalScore &rhs) {
  lhs -= rhs;
  return lhs;
}

int EvalScore::final(int phase) const {
  return (mid_game_ * phase + end_game_ * (MAX_PHASE - phase)) / MAX_PHASE;
}