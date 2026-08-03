#pragma once

#include "eval/score.h"
#include "game/bughouse.h"
#include "search/types.h"
#include <array>

constexpr int PHASE_WEIGHT[PIECE_TYPE_NO] = {0, 0, 1, 1, 2, 4, 0};

struct PawnInfo {
  std::array<Bitboard, COLOUR_NO> passed{};
  std::array<Bitboard, COLOUR_NO> isolated{};
  std::array<Bitboard, COLOUR_NO> doubled{};
};

struct AttackInfo {
  std::array<Bitboard, COLOUR_NO> attacks{};
  std::array<Bitboard, COLOUR_NO> kingZone{};
};

struct EvalContext {
  const Board &board;
  const SearchContext &search;

  PawnInfo pawn_info;
  AttackInfo attack_info;

  int phase = EvalScore::MAX_PHASE;
  // TODO: PartnerContext
};

EvalContext to_context(const Board &board, const SearchContext &search_context);

inline EvalContext to_context(const BughousePosition &position,
                              const SearchContext &search_context) {
  return to_context(position.boards[board_of(search_context.root_player)],
                    search_context);
}