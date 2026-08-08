#include "eval/score.h"

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

EvalScore EvalScore::scale(float weight) {
  mid_game_ *= weight;
  end_game_ *= weight;
  return *this;
}