#pragma once

#include "game/board.h"
#include "game/types.h"
#include <array>

inline constexpr int DIAG_DIRS[4] = {9, 7, -7, -9};
inline constexpr int ORTHO_DIRS[4] = {8, 1, -1, -8};

void init_attack_tables();

Bitboard knight_attacks(Square square);
Bitboard king_attacks(Square square);

Bitboard bishop_attacks(Square square, Bitboard bitboard);
Bitboard rook_attacks(Square square, Bitboard bitboard);
Bitboard queen_attacks(Square square, Bitboard bitboard);