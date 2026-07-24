#pragma once

#include "communication/strategy.h"

class TeamCoordinator {

public:
  TeamCoordinator(CommunicationStrategy &team_a, CommunicationStrategy &team_b);

  void update(const BughouseState &state);

  const SharedInfo &info(PlayerId player) const;

private:
  SharedInfo team_a_info_;
  SharedInfo team_b_info_;

  CommunicationStrategy &team_a_strat_;
  CommunicationStrategy &team_b_strat_;
};