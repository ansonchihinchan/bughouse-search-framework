#pragma once

#include "communication/context.h"
#include "communication/message.h"
#include "game/bughouse.h"

class Interpreter {
public:
  CommunicationContext interpret(const Message &latest,
                                 const BughousePosition &position,
                                 PlayerId root_player) const;
};