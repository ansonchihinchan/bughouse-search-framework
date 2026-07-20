#pragma once

#include "communication/shared.h"

#include "game/bughouse.h"

class CommunicationStrategy {

public:
  virtual ~CommunicationStrategy() = default;

  virtual void update(const BughouseState &state, PlayerId player,
                      SharedInfo &info) = 0;
};