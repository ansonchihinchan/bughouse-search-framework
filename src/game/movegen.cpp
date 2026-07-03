#include "game/movegen.h"

#include <bit>

namespace Attack {
// Precomputed knight/king attack tables
uint64_t KnightAttacks[SQUARE_NO];
uint64_t KingAttacks[SQUARE_NO];

bool tables_init = false;
void init_tables() {
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
const int DIAG_DIRS[4] = {9, 7, -7, -9};
const int ORTHO_DIRS[4] = {8, 1, -1, -8};

void add_pawn_moves(const Board &board, std::vector<Move> &moves) {
  Colour player = board.sideToMove;
  Colour opponent = flip(player);
  Bitboard all_bb = board.bitboard_all();
  Bitboard opp_bb = board.bitboard_colour(opponent);
  Bitboard player_pawns = board.bitboard_piece(make_piece(player, PAWN));

  int dir = (player == WHITE) ? 8 : -8;
  int start_rank = (player == WHITE) ? 1 : 6;
  int promo_rank = (player == WHITE) ? 7 : 0;

  Bitboard pawns = player_pawns;
  while (pawns) {
    int from = std::countr_zero(pawns);
    pawns &= pawns - 1;
    int to = from + dir;
    if (to < 0 || to >= 64)
      continue;

    // Single push
    if (!(all_bb & (1ULL << to))) {
      if (rank_of(to) == promo_rank) {
        for (auto pt : {KNIGHT, BISHOP, ROOK, QUEEN})
          moves.push_back(Move::promote(from, to, pt));
      } else {
        moves.push_back(Move::normal(from, to));
        // Double push
        if (rank_of(from) == start_rank) {
          int to2 = to + dir;
          if (!(all_bb & (1ULL << to2)))
            moves.push_back(Move::normal(from, to2));
        }
      }
    }

    // Captures
    for (int cap_dir : {dir + 1, dir - 1}) {
      int cap_to = from + cap_dir;
      if (cap_to < 0 || cap_to >= 64)
        continue;
      if (std::abs((cap_to & 7) - (from & 7)) != 1)
        continue; // wrap guard
      bool is_capture = (opp_bb & (1ULL << cap_to)) != 0;
      bool is_ep = (cap_to == board.enPassantSquare);
      if (is_capture || is_ep) {
        if (rank_of(cap_to) == promo_rank) {
          for (auto pt : {KNIGHT, BISHOP, ROOK, QUEEN})
            moves.push_back(Move::promote(from, cap_to, pt));
        } else if (is_ep) {
          moves.push_back(Move::en_passant(from, cap_to));
        } else {
          moves.push_back(Move::normal(from, cap_to));
        }
      }
    }
  }
}
} // namespace Attack

std::vector<Move> generate_moves(const Board &board, const Pocket *pocket) {
  Attack::init_tables();
  std::vector<Move> moves;
  moves.reserve(SQUARE_NO);

  Colour player = board.sideToMove;
  Bitboard player_bb = board.bitboard_colour(player);
  Bitboard all_bb = board.bitboard_all();

  Attack::add_pawn_moves(board, moves);

  // Knights
  Bitboard knights = board.bitboard_piece(make_piece(player, KNIGHT));
  while (knights) {
    int from = std::countr_zero(knights);
    knights &= knights - 1;
    Bitboard attack = Attack::KnightAttacks[from] & ~player;
    while (attack) {
      int to = std::countr_zero(attack);
      attack &= attack - 1;
      moves.push_back(Move::normal(from, to));
    }
  }

  // Diagonals
  Bitboard diagonals = board.bitboard_piece(make_piece(player, BISHOP)) |
                       board.bitboard_piece(make_piece(player, QUEEN));
  while (diagonals) {
    int from = std::countr_zero(diagonals);
    diagonals &= diagonals - 1;
    Bitboard attack =
        Attack::sliding(from, all_bb, Attack::DIAG_DIRS, 4) & ~player_bb;
    while (attack) {
      int to = std::countr_zero(attack);
      attack &= attack - 1;
      moves.push_back(Move::normal(from, to));
    }
  }

  // Orthogonals
  Bitboard orthogonals = board.bitboard_piece(make_piece(player, ROOK)) |
                         board.bitboard_piece(make_piece(player, QUEEN));
  while (orthogonals) {
    int from = std::countr_zero(orthogonals);
    orthogonals &= orthogonals - 1;
    Bitboard attack =
        Attack::sliding(from, all_bb, Attack::ORTHO_DIRS, 4) & ~player_bb;
    while (attack) {
      int to = std::countr_zero(attack);
      attack &= attack - 1;
      moves.push_back(Move::normal(from, to));
    }
  }

  // King
  Bitboard king = board.bitboard_piece(make_piece(player, KING));
  if (king) {
    int from = std::countr_zero(king);
    Bitboard attack = Attack::KingAttacks[from] & ~player_bb;
    while (attack) {
      int to = std::countr_zero(attack);
      attack &= attack - 1;
      moves.push_back(Move::normal(from, to));
    }

    // Castling
    if (player == WHITE) {
      if ((board.castlingRights & WHITE_OO) && !(all_bb & 0x60ULL) &&
          board.piece_on(7) == make_piece(WHITE, ROOK) &&
          !board.is_attacked(4, BLACK) && !board.is_attacked(5, BLACK) &&
          !board.is_attacked(6, BLACK))
        moves.push_back(Move::castling(4, 6));

      if ((board.castlingRights & WHITE_OOO) && !(all_bb & 0xEULL) &&
          board.piece_on(0) == make_piece(WHITE, ROOK) &&
          !board.is_attacked(4, BLACK) && !board.is_attacked(3, BLACK) &&
          !board.is_attacked(2, BLACK))
        moves.push_back(Move::castling(4, 2));
    } else {
      if ((board.castlingRights & BLACK_OO) &&
          !(all_bb & 0x6000000000000000ULL) &&
          board.piece_on(63) == make_piece(BLACK, ROOK) &&
          !board.is_attacked(60, WHITE) && !board.is_attacked(61, WHITE) &&
          !board.is_attacked(62, WHITE))
        moves.push_back(Move::castling(60, 62));

      if ((board.castlingRights & BLACK_OOO) &&
          !(all_bb & 0xE00000000000000ULL) &&
          board.piece_on(56) == make_piece(BLACK, ROOK) &&
          !board.is_attacked(60, WHITE) && !board.is_attacked(59, WHITE) &&
          !board.is_attacked(58, WHITE))
        moves.push_back(Move::castling(60, 58));
    }
  }

  // Drop moves
  if (pocket) {
    auto drops = generate_drops(board, *pocket);
    moves.insert(moves.end(), drops.begin(), drops.end());
  }

  return moves;
}

std::vector<Move> generate_drops(const Board &board, const Pocket &pocket) {
  std::vector<Move> moves;
  Bitboard empty = ~board.bitboard_all() & 0xFFFFFFFFFFFFFFFFULL;

  for (int pt = PAWN; pt <= QUEEN; pt++) {
    if (!pocket.contains(static_cast<PieceType>(pt)))
      continue;
    Bitboard targets = empty;
    // Pawns can't be dropped on rank 1 or rank 8
    if (pt == PAWN) {
      targets &= ~0xFF00000000000000ULL; // rank 8
      targets &= ~0x00000000000000FFULL; // rank 1
    }
    Bitboard t = targets;
    while (t) {
      int square = std::countr_zero(t);
      t &= t - 1;
      moves.push_back(Move::drop(static_cast<PieceType>(pt), square));
    }
  }
  return moves;
}

uint64_t perft(Board &board, int depth, const Pocket *pocket) {
  if (depth == 0)
    return 1;
  auto moves = generate_moves(board, pocket);
  uint64_t nodes = 0;
  for (auto move : moves) {
    if (!board.is_legal(move))
      continue;

    if (move.is_drop()) {
      UndoInfo undoInfo = board.make_drop(move.drop_pt, move.to);
      nodes += perft(board, depth - 1, pocket);
      board.undo_drop(move.to, undoInfo);
    } else {
      UndoInfo undoInfo = board.make_move(move);
      nodes += perft(board, depth - 1, pocket);
      board.undo_move(move, undoInfo);
    }
  }
  return nodes;
}