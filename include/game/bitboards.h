#pragma once

#include "game/board.h"

namespace Bitboards {

inline constexpr Bitboard FILE_A = 0x0101010101010101ULL;
inline constexpr Bitboard FILE_H = 0x8080808080808080ULL;

constexpr Bitboard file_mask(int file) {
    return FILE_A << file;
}

constexpr Bitboard rank_mask(int rank) {
    return 0xFFULL << (rank * 8);
}

constexpr Bitboard pawn_attacks(Bitboard pawns, Colour colour) {
    if (colour == WHITE) {
        return ((pawns << 7) & ~FILE_H) |
               ((pawns << 9) & ~FILE_A);
    }

    return ((pawns >> 7) & ~FILE_A) |
           ((pawns >> 9) & ~FILE_H);
}

} // namespace Bitboards