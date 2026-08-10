#pragma once

#include "eval/classical.h"
#include "eval/evaluator.h"
#include "game/bughouse.h"
#include "game/piece_value.h"

// Simple material-only evaluator
class BenchEvaluator : public Evaluator {
public:
  int evaluate(const BughousePosition &position, PlayerId root_player,
               const std::array<int64_t, PLAYER_NO> &remaining,
               const CommunicationContext &comm_context) const override {
    return classical_.evaluate(position.boards[board_of(root_player)],
                               colour_of(root_player));
  }

  bool is_noisy(const BughousePosition &position,
                PlayerId root_player) const override {
    return classical_.is_noisy(position.boards[board_of(root_player)]);
  }

private:
  ClassicalEvaluator classical_;
};
