#include "eval/bughouse.h"
#include "eval/bughouse/drop.h"
#include "eval/bughouse/exchange.h"
#include "eval/bughouse/initiative.h"
#include "eval/bughouse/king_danger.h"
#include "eval/bughouse/partner.h"
#include "eval/bughouse/pocket.h"
#include "eval/bughouse/prediction.h"
#include "game/attacks.h"
#include "game/movegen.h"

BughouseEvaluator::BughouseEvaluator() {
  features_.push_back(std::make_unique<DropEvaluator>());
  features_.push_back(std::make_unique<ExchangeEvaluator>());
  features_.push_back(std::make_unique<InitiativeEvaluator>());
  features_.push_back(std::make_unique<KingDangerEvaluator>());
  features_.push_back(std::make_unique<PartnerEvaluator>());
  features_.push_back(std::make_unique<PocketEvaluator>());
  features_.push_back(std::make_unique<PredictionEvaluator>());
}

int BughouseEvaluator::evaluate(
    const BughousePosition &position, PlayerId root_player,
    const std::array<int64_t, PLAYER_NO> &remaining,
    const CommunicationContext &comm_context) const {
  EvalContext eval_context =
      make_eval_context(position, root_player, remaining, comm_context);
  EvalScore score = EvalScore(0);
  for (const auto &feature : features_)
    score += feature->evaluate(eval_context);

  return score.final(eval_context.classical.phase) +
         classical_.evaluate(position.boards[board_of(root_player)],
                             colour_of_player(root_player));
}

bool BughouseEvaluator::is_noisy(const BughousePosition &position,
                                 PlayerId root_player) const {
  // TODO
}

float BughouseEvaluator::volatility(const BughousePosition &position,
                                    PlayerId root_player) const {
  // TODO
}