#pragma once

#include "eval/classical.h"
#include "eval/evaluator.h"
#include "eval/feature.h"
#include <memory>
#include <vector>

struct BughouseEvaluationConfig {
  bool include_partner_board = true;
  bool include_partner_pockets = true;
  bool include_communication = true;

  static BughouseEvaluationConfig independent();
  static BughouseEvaluationConfig request();
  static BughouseEvaluationConfig shared_value();
};

class BughouseEvaluator : public Evaluator {
public:
  explicit BughouseEvaluator(BughouseEvaluationConfig config = {});

  int evaluate(const BughousePosition &position, PlayerId root_player,
               const std::array<int64_t, PLAYER_NO> &remaining,
               const CommunicationContext &comm_context) const override;

  bool is_noisy(const BughousePosition &position,
                PlayerId root_player) const override;

  float volatility(const BughousePosition &position,
                   PlayerId root_player) const override;

private:
  BughouseEvaluationConfig config_;
  ClassicalEvaluator classical_;
  std::vector<std::unique_ptr<BughouseFeature>> features_;

  mutable std::array<uint64_t, COLOUR_NO> cached_partner_hash_{};
  mutable std::array<int, COLOUR_NO> cached_partner_score_{};
  mutable std::array<bool, COLOUR_NO> cached_partner_valid_{};
};