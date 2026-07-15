class EvalScore {
public:
  constexpr EvalScore(int mid_game = 0, int end_game = 0);

  EvalScore &operator+=(const EvalScore &);
  EvalScore &operator-=(const EvalScore &);

  friend EvalScore operator+(EvalScore lhs, const EvalScore &rhs);
  friend EvalScore operator-(EvalScore lhs, const EvalScore &rhs);

  int mid_game() const;
  int end_game() const;

  int final(int phase) const;

private:
  int mid_game_;
  int end_game_;
};