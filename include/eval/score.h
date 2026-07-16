#pragma once

class EvalScore {
public:
  constexpr EvalScore(int score) : mid_game_(score), end_game_(score) {}

  constexpr EvalScore(int mid_game, int end_game)
      : mid_game_(mid_game), end_game_(end_game) {}

  constexpr int mid_game() const { return mid_game_; }
  constexpr int end_game() const { return end_game_; }

  EvalScore &operator+=(const EvalScore &);
  EvalScore &operator-=(const EvalScore &);

  friend EvalScore operator+(EvalScore lhs, const EvalScore &rhs);
  friend EvalScore operator-(EvalScore lhs, const EvalScore &rhs);

  constexpr int final(int phase) const;

private:
  int mid_game_;
  int end_game_;
};