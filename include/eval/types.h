#pragma once

#include "communication/context.h"
#include "eval/score.h"
#include "game/bughouse.h"
#include "game/piece_value.h"
#include <array>

inline constexpr auto &PHASE_WEIGHT = PieceValue::PHASE_WEIGHT;

struct PawnInfo {
  std::array<Bitboard, COLOUR_NO> passed{};
  std::array<Bitboard, COLOUR_NO> isolated{};
  std::array<Bitboard, COLOUR_NO> doubled{};
};

struct AttackInfo {
  std::array<Bitboard, COLOUR_NO> attacks{};
  std::array<Bitboard, COLOUR_NO> kingZone{};
};

struct ClassicalContext {
  const Board &board;

  PawnInfo pawn_info;
  AttackInfo attack_info;

  int phase = EvalScore::MAX_PHASE;
};

ClassicalContext to_classical_context(const Board &board);

struct BughouseContext {
  const std::array<Pocket, PLAYER_NO> &pockets;
  PlayerId root_player;
  std::array<int64_t, PLAYER_NO> remaining;

  const Pocket &own_pocket() const { return pockets[to_int(root_player)]; }
  const Pocket &opp_pocket() const {
    return pockets[to_int(next_player(root_player))];
  }
  const Pocket &partner_pocket() const {
    return pockets[to_int(partner_of(root_player))];
  }
};

BughouseContext to_bughouse_context(const BughousePosition &position,
                                    PlayerId root_player);

struct EvalContext {
  ClassicalContext classical;
  BughouseContext bughouse;
  CommunicationContext communication;
};