#include "game/attacks.h"
#include "game/bitboards.h"
#include <mutex>

namespace {
// Precomputed knight/king attack tables
uint64_t KnightAttacks[SQUARE_NO];
uint64_t KingAttacks[SQUARE_NO];

std::once_flag tables_init;

template <size_t N>
Bitboard sliding(Square square, Bitboard bitboard,
                 const std::array<int, N> dirs) {
  Bitboard attack = 0;
  for (int i = 0; i < N; i++) {
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
  std::call_once(tables_init, []() {
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
  });
}

Bitboard pawn_attacks(Bitboard pawns, Colour colour) {
  if (colour == WHITE) {
    return ((pawns << 7) & ~Bitboards::FILE_H) |
           ((pawns << 9) & ~Bitboards::FILE_A);
  }

  return ((pawns >> 7) & ~Bitboards::FILE_A) |
         ((pawns >> 9) & ~Bitboards::FILE_H);
}

Bitboard knight_attacks(Square square) { return KnightAttacks[square]; }

Bitboard king_attacks(Square square) { return KingAttacks[square]; }

Bitboard bishop_attacks(Square square, Bitboard bitboard) {
  return sliding(square, bitboard, DIAG_DIRS);
}

Bitboard rook_attacks(Square square, Bitboard bitboard) {
  return sliding(square, bitboard, ORTHO_DIRS);
}

Bitboard queen_attacks(Square square, Bitboard bitboard) {
  return bishop_attacks(square, bitboard) | rook_attacks(square, bitboard);
}

bool drop_gives_check(const Board &board, PieceType pt, Square to,
                      Colour colour) {
  Colour enemy = flip(colour);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard occ = board.bitboard_all();

  switch (pt) {
  case KNIGHT:
    return (knight_attacks(to) & king_bb) != 0;
  case BISHOP:
    return (bishop_attacks(to, occ) & king_bb) != 0;
  case ROOK:
    return (rook_attacks(to, occ) & king_bb) != 0;
  case QUEEN:
    return ((bishop_attacks(to, occ) | rook_attacks(to, occ)) & king_bb) != 0;
  case PAWN: {
    int file_diff = std::abs(file_of(ksq) - file_of(to));
    int rank_diff = rank_of(ksq) - rank_of(to);
    int expected_rank_diff = (colour == WHITE) ? 1 : -1;
    return file_diff == 1 && rank_diff == expected_rank_diff;
  }
  default:
    return false;
  }
}

bool move_gives_check(const Board &board, Move move, Colour mover) {
  if (move.is_drop())
    return drop_gives_check(board, move.drop_pt, move.to, mover);

  Colour enemy = flip(mover);
  Bitboard king_bb = board.bitboard_piece(make_piece(enemy, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  PieceType pt = board.piece_on(move.from).type;
  if (move.type == PROMOTE)
    pt = move.promote_pt;
  if (move.type == CASTLE)
    pt = ROOK;

  Square from_sq = move.from;
  Square to_sq = move.to;
  if (move.type == CASTLE) {
    bool kingside = move.to > move.from;
    from_sq = to_square(kingside ? 7 : 0, rank_of(move.from));
    to_sq = to_square(kingside ? 5 : 3, rank_of(move.from));
  }

  Bitboard occ = board.bitboard_all();
  occ &= ~(1ULL << from_sq);
  occ |= (1ULL << to_sq);

  switch (pt) {
  case KNIGHT:
    return (knight_attacks(to_sq) & king_bb) != 0;
  case BISHOP:
    return (bishop_attacks(to_sq, occ) & king_bb) != 0;
  case ROOK:
    return (rook_attacks(to_sq, occ) & king_bb) != 0;
  case QUEEN:
    return ((bishop_attacks(to_sq, occ) | rook_attacks(to_sq, occ)) &
            king_bb) != 0;
  case PAWN: {
    int file_diff = std::abs(file_of(ksq) - file_of(to_sq));
    int rank_diff = rank_of(ksq) - rank_of(to_sq);
    int expected_rank_diff = (mover == WHITE) ? 1 : -1;
    return file_diff == 1 && rank_diff == expected_rank_diff;
  }
  default:
    return false;
  }
}