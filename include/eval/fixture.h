#pragma once

#include "communication/message.h"
#include "eval/types.h"
#include "game/types.h"

// For tests
struct Fixture {
  Board board;
  std::array<Pocket, PLAYER_NO> pockets{};
  std::array<int64_t, PLAYER_NO> remaining{};
  PartnerContext partner{};
  PredictionSummary prediction{};

  explicit Fixture(const std::string &fen) { board.load_fen(fen); }

  EvalContext context(PlayerId root_player) const {
    return EvalContext{to_classical_context(board),
                       BughouseContext{pockets, root_player, remaining},
                       CommunicationContext{partner, prediction}};
  }
};

constexpr const char *BARE_KINGS = "4k3/8/8/8/8/8/8/4K3 w - - 0 1";