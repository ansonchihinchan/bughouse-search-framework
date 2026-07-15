#pragma once

#include "game/bughouse.h"
#include "search/types.h"

struct MaterialInfo {
  int value[2];
  int phase;
};

struct PawnInfo {
  Bitboard passed[2];
  Bitboard isolated[2];
  Bitboard doubled[2];
};

struct AttackInfo {
  Bitboard attacks[2];
  Bitboard kingZone[2];
};

struct EvalContext {
  const BughousePosition &position;
  const SearchContext &search;

  MaterialInfo material_info;
  PawnInfo pawn_info;
  AttackInfo attack_info;
};

constexpr EvalContext to_context(const BughousePosition &position,
                                 const SearchContext &search) {
  // TODO
}