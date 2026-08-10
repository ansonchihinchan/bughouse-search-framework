#pragma once

#include "communication/message.h"
#include "game/bughouse.h"
#include "search/types.h"

// Runs once per real move after a move is chosen
// Inspects root player's resulting position and search output
// Decides the message to partner

// TODO
class Generator {
public:
  Message generate_message(const SearchResult &search_result,
                           const BughousePosition &position,
                           PlayerId root_player);
};