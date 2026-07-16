#include "eval/types.h"
#include "game/attacks.h"
#include <algorithm>
#include <bit>

namespace {

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
Bitboard file_mask(int file) { return FILE_A_BB << file; }

Bitboard forward_mask(Square sq, Colour colour) {
  int file = file_of(sq);
  int rank = rank_of(sq);

  Bitboard files = file_mask(file);
  if (file > 0)
    files |= file_mask(file - 1);
  if (file < 7)
    files |= file_mask(file + 1);

  Bitboard ranks_ahead = 0;
  if (colour == WHITE) {
    for (int r = rank + 1; r < 8; r++)
      ranks_ahead |= 0xFFULL << (r * 8);
  } else {
    for (int r = rank - 1; r >= 0; r--)
      ranks_ahead |= 0xFFULL << (r * 8);
  }
  return files & ranks_ahead;
}

void compute_pawn_info(const Board &board, Colour colour, Bitboard &passed,
                       Bitboard &isolated, Bitboard &doubled) {
  Bitboard friendly = board.bitboard_piece(make_piece(colour, PAWN));
  Bitboard enemy = board.bitboard_piece(make_piece(flip(colour), PAWN));

  passed = isolated = doubled = 0;

  Bitboard pawns = friendly;
  while (pawns) {
    Square sq = static_cast<Square>(std::countr_zero(pawns));
    pawns &= pawns - 1;

    if (!(forward_mask(sq, colour) & enemy))
      passed |= 1ULL << sq;

    int file = file_of(sq);
    Bitboard adjacent_files = 0;
    if (file > 0)
      adjacent_files |= file_mask(file - 1);
    if (file < 7)
      adjacent_files |= file_mask(file + 1);
    if (!(adjacent_files & friendly))
      isolated |= 1ULL << sq;

    // Flags every pawn on a doubled file
    if (std::popcount(file_mask(file) & friendly) > 1)
      doubled |= 1ULL << sq;
  }
}

Bitboard pawn_attacks(Bitboard pawns, Colour colour) {
  if (colour == WHITE)
    return ((pawns << 7) & ~0x8080808080808080ULL) |
           ((pawns << 9) & ~0x0101010101010101ULL);
  return ((pawns >> 7) & ~0x0101010101010101ULL) |
         ((pawns >> 9) & ~0x8080808080808080ULL);
}

Bitboard compute_attacks(const Board &board, Colour colour) {
  Bitboard occ = board.bitboard_all();
  Bitboard attacks =
      pawn_attacks(board.bitboard_piece(make_piece(colour, PAWN)), colour);

  Bitboard knights = board.bitboard_piece(make_piece(colour, KNIGHT));
  while (knights) {
    Square sq = static_cast<Square>(std::countr_zero(knights));
    knights &= knights - 1;
    attacks |= knight_attacks(sq);
  }

  Bitboard diag_sliders = board.bitboard_piece(make_piece(colour, BISHOP)) |
                          board.bitboard_piece(make_piece(colour, QUEEN));
  while (diag_sliders) {
    Square sq = static_cast<Square>(std::countr_zero(diag_sliders));
    diag_sliders &= diag_sliders - 1;
    attacks |= bishop_attacks(sq, occ);
  }

  Bitboard ortho_sliders = board.bitboard_piece(make_piece(colour, ROOK)) |
                           board.bitboard_piece(make_piece(colour, QUEEN));
  while (ortho_sliders) {
    Square sq = static_cast<Square>(std::countr_zero(ortho_sliders));
    ortho_sliders &= ortho_sliders - 1;
    attacks |= rook_attacks(sq, occ);
  }

  Bitboard king = board.bitboard_piece(make_piece(colour, KING));
  if (king)
    attacks |= king_attacks(static_cast<Square>(std::countr_zero(king)));

  return attacks;
}

Bitboard king_zone(const Board &board, Colour colour) {
  Bitboard king = board.bitboard_piece(make_piece(colour, KING));
  if (!king)
    return 0;
  Square ksq = static_cast<Square>(std::countr_zero(king));
  return king_attacks(ksq) | (1ULL << ksq);
}

} // namespace

EvalContext to_context(const BughousePosition &position,
                       const SearchContext &search) {
  EvalContext context{position, search, {}, {}, {}};
  init_attack_tables();

  int board_phase[BOARD_NO];

  for (int b = 0; b < BOARD_NO; b++) {
    const Board &board = position.boards[b];

    for (Colour colour : {WHITE, BLACK}) {
      int phase = 0;
      for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN})
        phase += PHASE_WEIGHT[pt] *
                 std::popcount(board.bitboard_piece(make_piece(colour, pt)));
      context.material_info.boards[b].phase[colour] = phase;

      compute_pawn_info(board, colour, context.pawn_info.passed[b][colour],
                        context.pawn_info.isolated[b][colour],
                        context.pawn_info.doubled[b][colour]);

      context.attack_info.attacks[b][colour] = compute_attacks(board, colour);
      context.attack_info.kingZone[b][colour] = king_zone(board, colour);
    }

    board_phase[b] = std::min(EvalScore::MAX_PHASE,
                              context.material_info.boards[b].phase[WHITE] +
                                  context.material_info.boards[b].phase[BLACK]);
  }

  context.material_info.phase = (board_phase[0] + board_phase[1]) / 2;
  return context;
}