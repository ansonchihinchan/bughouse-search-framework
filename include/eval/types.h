#pragma once

#include "eval/score.h"
#include "game/bughouse.h"
#include "search/types.h"
#include <array>

constexpr int PHASE_WEIGHT[PIECE_TYPE_NO] = {0, 0, 1, 1, 2, 4, 0};

struct BoardMaterial {
  int phase[COLOUR_NO] = {0, 0};
};

struct MaterialInfo {
  std::array<BoardMaterial, BOARD_NO> boards{};
  int phase = EvalScore::MAX_PHASE;
};

struct PawnInfo {
  std::array<std::array<Bitboard, COLOUR_NO>, BOARD_NO> passed{};
  std::array<std::array<Bitboard, COLOUR_NO>, BOARD_NO> isolated{};
  std::array<std::array<Bitboard, COLOUR_NO>, BOARD_NO> doubled{};
};

struct AttackInfo {
  std::array<std::array<Bitboard, COLOUR_NO>, BOARD_NO> attacks{};
  std::array<std::array<Bitboard, COLOUR_NO>, BOARD_NO> kingZone{};
};

struct EvalContext {
  const BughousePosition &position;
  const SearchContext &search;

  MaterialInfo material_info;
  PawnInfo pawn_info;
  AttackInfo attack_info;
};

EvalContext to_context(const BughousePosition &position,
                       const SearchContext &search);