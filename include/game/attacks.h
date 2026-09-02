#pragma once

#include "game/board.h"
#include "game/types.h"
#include <array>

inline constexpr std::array<int, 4> DIAG_DIRS{9, 7, -7, -9};
inline constexpr std::array<int, 4> ORTHO_DIRS{8, 1, -1, -8};

void init_attack_tables();

Bitboard pawn_attacks(Bitboard pawns, Colour colour);

Bitboard knight_attacks(Square square);
Bitboard king_attacks(Square square);

Bitboard bishop_attacks(Square square, Bitboard bitboard);
Bitboard rook_attacks(Square square, Bitboard bitboard);
Bitboard queen_attacks(Square square, Bitboard bitboard);

Bitboard piece_attacks(PieceType pt, Colour colour, Square sq, Bitboard occ);

bool move_gives_check(const Board &board, Move move, Colour colour);
bool drop_gives_check(const Board &board, PieceType pt, Square to,
                      Colour colour);

struct DropCheckMasks {
  Bitboard pawn = 0;
  Bitboard knight = 0;
  Bitboard bishop = 0;
  Bitboard rook = 0;
  Bitboard queen = 0;

  Bitboard for_piece(PieceType pt) const;
};

// Empty legal squares from which a dropped piece would geometrically check the
// opposing king
DropCheckMasks drop_check_masks(const Board &board, Colour colour);
Bitboard drop_check_squares(const Board &board, PieceType pt, Colour colour);
