#include "game/attacks.h"

namespace {
// Precomputed knight/king attack tables
uint64_t KnightAttacks[SQUARE_NO];
uint64_t KingAttacks[SQUARE_NO];

bool tables_init = false;

Bitboard sliding(Square square, Bitboard bitboard, const int *dirs, int n) {
  Bitboard attack = 0;
  for (int i = 0; i < n; i++) {
    int cur = square + dirs[i];
    while (cur >= 0 && cur < 64 &&
           std::abs((cur & 7) - ((cur - dirs[i]) & 7)) <= 1) {
      attack |= 1ULL << cur;
      if (bitboard & (1ULL << cur))
        break;
      cur += dirs[i];
    }
  }
  return attack;
}
} // namespace

void init_attack_tables() {
  if (tables_init)
    return;
  tables_init = true;
  const int knight_offs[] = {17, 15, 10, 6, -6, -10, -15, -17};
  const int king_offs[] = {9, 8, 7, 1, -1, -7, -8, -9};
  for (int s = 0; s < 64; s++) {
    for (int off : knight_offs) {
      int t = s + off;
      if (t >= 0 && t < 64 && std::abs((t & 7) - (s & 7)) <= 2)
        KnightAttacks[s] |= 1ULL << t;
    }
    for (int off : king_offs) {
      int t = s + off;
      if (t >= 0 && t < 64 && std::abs((t & 7) - (s & 7)) <= 1)
        KingAttacks[s] |= 1ULL << t;
    }
  }
}

Bitboard knight_attacks(Square square) { return KnightAttacks[square]; }

Bitboard king_attacks(Square square) { return KingAttacks[square]; }

Bitboard bishop_attacks(Square square, Bitboard bitboard) {
  return sliding(square, bitboard, DIAG_DIRS, 4);
}

Bitboard rook_attacks(Square square, Bitboard bitboard) {
  return sliding(square, bitboard, ORTHO_DIRS, 4);
}

Bitboard queen_attacks(Square square, Bitboard bitboard) {
  return bishop_attacks(square, bitboard) | rook_attacks(square, bitboard);
}