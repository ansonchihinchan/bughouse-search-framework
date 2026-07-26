#include "search/see.h"
#include "game/attacks.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace {
Bitboard pawn_attackers_to(const Board &board, Square square, Colour colour,
                           Bitboard bitboard) {
  Bitboard pawns = board.bitboard_piece(make_piece(colour, PAWN)) & bitboard;
  Bitboard result = 0;
  if (colour == WHITE) {
    if (file_of(square) != 0) {
      Square s = square - 9;
      if (s >= 0 && (pawns & (1ULL << s)))
        result |= 1ULL << s;
    }
    if (file_of(square) != 7) {
      Square s = square - 7;
      if (s >= 0 && (pawns & (1ULL << s)))
        result |= 1ULL << s;
    }
  } else {
    if (file_of(square) != 0) {
      Square s = square + 7;
      if (s < 64 && (pawns & (1ULL << s)))
        result |= 1ULL << s;
    }
    if (file_of(square) != 7) {
      Square s = square + 9;
      if (s < 64 && (pawns & (1ULL << s)))
        result |= 1ULL << s;
    }
  }
  return result;
}

Bitboard attackers_to(const Board &board, Square square, Bitboard bitboard) {
  Bitboard attackers = 0;

  attackers |= pawn_attackers_to(board, square, WHITE, bitboard);
  attackers |= pawn_attackers_to(board, square, BLACK, bitboard);

  Bitboard knights = board.bitboard_piece(make_piece(WHITE, KNIGHT)) |
                     board.bitboard_piece(make_piece(BLACK, KNIGHT));
  attackers |= knight_attacks(square) & knights & bitboard;

  Bitboard kings = board.bitboard_piece(make_piece(WHITE, KING)) |
                   board.bitboard_piece(make_piece(BLACK, KING));
  attackers |= king_attacks(square) & kings & bitboard;

  Bitboard diagSliders = board.bitboard_piece(make_piece(WHITE, BISHOP)) |
                         board.bitboard_piece(make_piece(BLACK, BISHOP)) |
                         board.bitboard_piece(make_piece(WHITE, QUEEN)) |
                         board.bitboard_piece(make_piece(BLACK, QUEEN));
  attackers |= bishop_attacks(square, bitboard) & diagSliders & bitboard;

  Bitboard orthoSliders = board.bitboard_piece(make_piece(WHITE, ROOK)) |
                          board.bitboard_piece(make_piece(BLACK, ROOK)) |
                          board.bitboard_piece(make_piece(WHITE, QUEEN)) |
                          board.bitboard_piece(make_piece(BLACK, QUEEN));
  attackers |= rook_attacks(square, bitboard) & orthoSliders & bitboard;

  return attackers;
}

PieceType least_valuable_attacker(const Board &board, Bitboard attackers,
                                  Colour colour, Square &from) {
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
    Bitboard bb = attackers & board.bitboard_piece(make_piece(colour, pt));
    if (bb) {
      from = static_cast<Square>(std::countr_zero(bb));
      return pt;
    }
  }
  return NO_PIECE_TYPE;
}

int effective_value(PieceType pt) {
  return SEE::PIECE_VALUE[pt] + SEE::POCKET_BONUS[pt];
}

// Pinned
bool king_exposed(const Board &board, Move move) {
  Colour side = board.sideToMove;
  Bitboard king_bb = board.bitboard_piece(make_piece(side, KING));
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Square from = move.from;

  bool orthogonal =
      file_of(ksq) == file_of(from) || rank_of(ksq) == rank_of(from);
  bool diagonal = std::abs(file_of(ksq) - file_of(from)) ==
                  std::abs(rank_of(ksq) - rank_of(from));

  if (!orthogonal && !diagonal)
    return false;

  Bitboard occ = board.bitboard_all();

  Bitboard ray_before =
      orthogonal ? rook_attacks(ksq, occ) : bishop_attacks(ksq, occ);
  if (!(ray_before & (1ULL << from)))
    return false;

  occ &= ~(1ULL << from);

  if (move.type == EN_PASSANT) {
    Square ep_sq = to_square(file_of(move.to), rank_of(move.from));
    occ &= ~(1ULL << ep_sq);
  }

  Colour enemy = flip(side);

  if (orthogonal) {
    Bitboard orthoRQ = board.bitboard_piece(make_piece(enemy, ROOK)) |
                       board.bitboard_piece(make_piece(enemy, QUEEN));
    return (rook_attacks(ksq, occ) & orthoRQ) != 0;
  } else {
    Bitboard diagBQ = board.bitboard_piece(make_piece(enemy, BISHOP)) |
                      board.bitboard_piece(make_piece(enemy, QUEEN));
    return (bishop_attacks(ksq, occ) & diagBQ) != 0;
  }
}

} // namespace

namespace SEE {

Result see_result(const Board &board, Move move) {
  init_attack_tables();
  Result result;

  if (move.is_drop())
    return result;

  Piece captured = board.piece_on(move.to);
  if (move.type == EN_PASSANT)
    captured = make_piece(flip(board.sideToMove), PAWN);

  if (captured.is_empty())
    return result;

  Colour mover_colour = board.sideToMove;
  PieceType occupant = board.piece_on(move.from).type;

  Bitboard occ = board.bitboard_all();
  occ &= ~(1ULL << move.from);

  if (move.type == EN_PASSANT) {
    Square ep_sq = to_square(file_of(move.to), rank_of(move.from));
    occ &= ~(1ULL << ep_sq);
  }

  occ |= (1ULL << move.to);

  PieceType captured_list[32];
  int n = 0;
  captured_list[n++] = captured.type;

  int promotion_bonus = 0;
  if (move.type == PROMOTE) {
    promotion_bonus = PIECE_VALUE[move.promote_pt] - PIECE_VALUE[PAWN];
    occupant = move.promote_pt;
  }

  Colour side = flip(mover_colour);
  Bitboard attackers = attackers_to(board, move.to, occ);

  bool first_recapture_checked = false;
  result.undefended = true;

  while (n < 32) {
    Square from_sq = -1;
    PieceType lva =
        least_valuable_attacker(board, attackers & occ, side, from_sq);

    if (!first_recapture_checked) {
      first_recapture_checked = true;
      result.undefended = (lva == NO_PIECE_TYPE);
    }

    if (lva == NO_PIECE_TYPE)
      break;

    captured_list[n++] = occupant;

    occ ^= 1ULL << from_sq;
    attackers = attackers_to(board, move.to, occ);
    occupant = lva;
    side = flip(side);
  }

  int gain[32];
  gain[0] = effective_value(captured_list[0]);
  for (int i = 1; i < n; i++)
    gain[i] = effective_value(captured_list[i]) - gain[i - 1];

  for (int i = n - 1; i > 0; i--)
    gain[i - 1] = -std::max(-gain[i - 1], gain[i]);

  result.score = gain[0] + promotion_bonus;

  if (!move.is_drop() && king_exposed(board, move)) {
    result.king_exposed = true;
    result.score -= PIECE_VALUE[QUEEN] / 6;
  }

  return result;
}

Result see_drop_result(const Board &board, PieceType pt, Square to) {
  init_attack_tables();

  Result result;

  Bitboard occ = board.bitboard_all() | (1ULL << to);

  PieceType captured_list[32];
  int n = 0;
  captured_list[n++] = pt;

  PieceType occupant = pt;
  Colour side = flip(board.sideToMove);

  Bitboard attackers = attackers_to(board, to, occ);
  bool first_recapture_checked = false;
  result.undefended = true;

  while (n < 32) {
    Square from_sq = -1;
    PieceType lva =
        least_valuable_attacker(board, attackers & occ, side, from_sq);

    if (!first_recapture_checked) {
      first_recapture_checked = true;
      result.undefended = (lva == NO_PIECE_TYPE);
    }

    if (lva == NO_PIECE_TYPE)
      break;

    captured_list[n++] = occupant;
    occ ^= 1ULL << from_sq;
    attackers = attackers_to(board, to, occ);
    occupant = lva;
    side = flip(side);
  }

  int gain[32];
  gain[0] = 0;
  for (int i = 1; i < n; i++)
    gain[i] = effective_value(captured_list[i]) - gain[i - 1];

  for (int i = n - 1; i > 0; i--)
    gain[i - 1] = -std::max(-gain[i - 1], gain[i]);

  result.score = gain[0];
  return result;
}

} // namespace SEE