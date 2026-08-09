#include "communication/context.h"

PartnerContext make_partner_context(const BughousePosition &position,
                                    PlayerId partner) {
  // TODO
}

PredictionSummary make_prediction_summary(const BughousePosition &position,
                                          PlayerId root_player,
                                          Message message) {
  // TODO
}

CommunicationContext
make_communication_context(const BughousePosition &position,
                           PlayerId root_player, const Channel &channel) {
  CommunicationContext context;
  const PlayerId partner = partner_of(root_player);
  context.message = channel.latest(partner);
  context.partner = make_partner_context(position, partner);
  context.prediction =
      make_prediction_summary(position, root_player, context.message);
  return context;
}