#include "eval/classical.h"
#include "eval/evaluator.h"
#include "eval/feature.h"
#include <vector>

class BughouseEvaluator : public Evaluator {
public:
  int evaluate(const BughousePosition &position, PlayerId root_player,
               const std::array<int64_t, PLAYER_NO> &remaining) const override;

  bool is_noisy(const BughousePosition &position,
                PlayerId root_player) const override;

  float volatility(const BughousePosition &position,
                   PlayerId root_player) const override;

private:
  ClassicalEvaluator classical_;
  std::vector<std::unique_ptr<BughouseFeature>> features_;
};