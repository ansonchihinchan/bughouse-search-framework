#pragma once

#include "game/board.h"
#include "game/types.h"
#include <array>

inline constexpr std::array<int, 4> DIAG_DIRS{9, 7, -7, -9};
inline constexpr std::array<int, 4> ORTHO_DIRS{8, 1, -1, -8};

void init_attack_tables();

Bitboard knight_attacks(Square square);
Bitboard king_attacks(Square square);

Bitboard bishop_attacks(Square square, Bitboard bitboard);
Bitboard rook_attacks(Square square, Bitboard bitboard);
Bitboard queen_attacks(Square square, Bitboard bitboard);

bool move_gives_check(const Board &board, Move move, Colour colour);
bool drop_gives_check(const Board &board, PieceType pt, Square to, Colour colour);