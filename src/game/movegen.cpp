#include "game/movegen.h"
#include "game/attacks.h"

#include <bit>

namespace {
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
} // namespace

std::vector<Move> generate_pseudo_legal_moves(const Board &board,
                                              const Pocket *pocket) {
  init_attack_tables();
  std::vector<Move> moves;
  moves.reserve(SQUARE_NO);

  Colour player = board.sideToMove;
  Bitboard player_bb = board.bitboard_colour(player);
  Bitboard all_bb = board.bitboard_all();

  add_pawn_moves(board, moves);

  // Knights
  Bitboard knights = board.bitboard_piece(make_piece(player, KNIGHT));
  while (knights) {
    int from = std::countr_zero(knights);
    knights &= knights - 1;
    Bitboard attack = knight_attacks(from) & ~player_bb;
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
    Bitboard attack = bishop_attacks(from, all_bb) & ~player_bb;
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
    Bitboard attack = rook_attacks(from, all_bb) & ~player_bb;
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
    Bitboard attack = king_attacks(from) & ~player_bb;
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
    auto drops = generate_drop_moves(board, *pocket);
    moves.insert(moves.end(), drops.begin(), drops.end());
  }

  return moves;
}

std::vector<Move> generate_legal_moves(const BughousePosition &position,
                                       PlayerId player) {
  const Board &board = position.boards[board_of(player)];

  auto moves =
      generate_pseudo_legal_moves(board, &position.pockets[to_int(player)]);

  std::erase_if(moves, [&](const Move &move) { return !board.is_legal(move); });

  return moves;
}

std::vector<Move> generate_drop_moves(const Board &board,
                                      const Pocket &pocket) {
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
  auto moves = generate_pseudo_legal_moves(board, pocket);
  uint64_t nodes = 0;
  for (auto move : moves) {
    if (!board.is_legal(move))
      continue;

    if (move.is_drop()) {
      BoardUndo undo = board.make_drop(move.drop_pt, move.to);
      nodes += perft(board, depth - 1, pocket);
      board.undo_drop(move.to, undo);
    } else {
      BoardUndo undo = board.make_move(move);
      nodes += perft(board, depth - 1, pocket);
      board.undo_move(move, undo);
    }
  }
  return nodes;
}