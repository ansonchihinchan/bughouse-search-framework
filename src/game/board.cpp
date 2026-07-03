#include "game/board.h"
#include "game/movegen.h"
#include <bit>
#include <cassert>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>

#define CASTLING_RIGHTS_NO 16
#define ENPASSANT_FILE_NO 8
#define a8 56

namespace Zobrist {
// piece × square
uint64_t pieceSquare[PIECE_NO][SQUARE_NO];
uint64_t side;
uint64_t castlingRights[CASTLING_RIGHTS_NO];
uint64_t enPassantFile[ENPASSANT_FILE_NO];

std::once_flag init_flag;

void init() {
  std::mt19937_64 rng(0xAC0123456789ULL);
  for (int piece = 0; piece < PIECE_NO; piece++) {
    for (int square = 0; square < SQUARE_NO; square++) {
      pieceSquare[piece][square] = rng();
    }
  }
  side = rng();
  for (int cr = 0; cr < CASTLING_RIGHTS_NO; cr++) {
    castlingRights[cr] = rng();
  }
  for (int epf = 0; epf < ENPASSANT_FILE_NO; epf++) {
    enPassantFile[epf] = rng();
  }
}

void ensure_init() { std::call_once(init_flag, init); }

} // namespace Zobrist

Board::Board() { reset(); }
Board::Board(const std::string &fen) { load_fen(fen); }

void Board::reset() {
  load_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Board::put_piece(Piece piece, Square square) {
  squares[square] = piece;
  bitboards[piece.index()] |= 1ULL << square;
  hash ^= Zobrist::pieceSquare[piece.index()][square];
}

void Board::remove_piece(Square square) {
  Piece piece = squares[square];
  squares[square].type = NO_PIECE_TYPE;
  bitboards[piece.index()] &= ~(1ULL << square);
  hash ^= Zobrist::pieceSquare[piece.index()][square];
}

void Board::move_piece(Square from, Square to) {
  Piece piece = squares[from];
  remove_piece(from);
  put_piece(piece, to);
}

Bitboard Board::bitboard_colour(Colour colour) const {
  Bitboard result = 0;
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
    result |= bitboards[make_piece(colour, pt).index()];
  return result;
}

uint64_t Board::bitboard_all() const {
  return bitboard_colour(WHITE) | bitboard_colour(BLACK);
}

bool Board::load_fen(const std::string &fen) {
  Zobrist::ensure_init();

  std::istringstream ss(fen);
  std::string board_str, side_str, castling_str, ep_str;
  int hm = 0, fm = 1;

  if (!(ss >> board_str >> side_str >> castling_str >> ep_str >> hm >> fm))
    return false;

  if (side_str != "w" && side_str != "b")
    return false;

  if (castling_str != "-")
    for (char c : castling_str)
      if (c != 'K' && c != 'Q' && c != 'k' && c != 'q')
        return false;

  if (ep_str != "-" && (ep_str.size() != 2 || ep_str[0] < 'a' ||
                        ep_str[0] > 'h' || ep_str[1] < '1' || ep_str[1] > '8'))
    return false;

  std::array<Piece, SQUARE_NO> new_squares{};
  std::array<Bitboard, PIECE_NO> new_bitboards{};
  uint64_t new_hash = 0;
  int rank = 7, file = 0, white_kings = 0, black_kings = 0;

  for (char c : board_str) {
    if (c == '/') {
      if (file != 8)
        return false;
      rank--;
      file = 0;
      if (rank < 0)
        return false;
      continue;
    }
    if (c >= '1' && c <= '8') {
      file += c - '0';
      if (file > 8)
        return false;
      continue;
    }
    static const std::string piece_chars = "PNBRQKpnbrqk";
    size_t idx = piece_chars.find(c);
    if (idx == std::string::npos || file >= 8)
      return false;

    Colour colour = (idx < 6) ? WHITE : BLACK;
    PieceType pt = static_cast<PieceType>((idx % 6) + 1);
    Piece piece = make_piece(colour, pt);
    Square sq = to_square(file, rank);

    new_squares[sq] = piece;
    new_bitboards[piece.index()] |= 1ULL << sq;
    new_hash ^= Zobrist::pieceSquare[piece.index()][sq];

    if (pt == KING)
      (colour == WHITE ? white_kings : black_kings)++;

    file++;
  }
  if (rank != 0 || file != 8 || white_kings != 1 || black_kings != 1)
    return false;

  squares = new_squares;
  bitboards = new_bitboards;
  hash = new_hash;
  halfMove = hm;
  fullMove = fm;

  sideToMove = (side_str == "b") ? BLACK : WHITE;
  if (sideToMove == BLACK)
    hash ^= Zobrist::side;

  castlingRights = NO_CASTLING;
  for (char c : castling_str) {
    if (c == 'K')
      castlingRights |= WHITE_OO;
    if (c == 'Q')
      castlingRights |= WHITE_OOO;
    if (c == 'k')
      castlingRights |= BLACK_OO;
    if (c == 'q')
      castlingRights |= BLACK_OOO;
  }
  hash ^= Zobrist::castlingRights[castlingRights];

  enPassantSquare = -1;
  if (ep_str != "-") {
    enPassantSquare = to_square(ep_str[0] - 'a', ep_str[1] - '1');
    hash ^= Zobrist::enPassantFile[file_of(enPassantSquare)];
  }
  return true;
}

std::string Board::to_fen() const {
  std::string result;
  for (int rank = 7; rank >= 0; rank--) {
    int empty_count = 0;
    for (int file = 0; file < 8; file++) {
      Piece piece = squares[to_square(file, rank)];
      if (piece.is_empty()) {
        empty_count++;
        continue;
      }
      if (empty_count > 0) {
        result += ('0' + empty_count);
        empty_count = 0;
      }
      result += piece.to_char();
    }
    if (empty_count > 0)
      result += ('0' + empty_count);
    if (rank > 0)
      result += '/';
  }
  result += (sideToMove == WHITE) ? " w " : " b ";
  if (castlingRights == NO_CASTLING)
    result += '-';
  else {
    if (castlingRights & WHITE_OO)
      result += 'K';
    if (castlingRights & WHITE_OOO)
      result += 'Q';
    if (castlingRights & BLACK_OO)
      result += 'k';
    if (castlingRights & BLACK_OOO)
      result += 'q';
  }
  result += (enPassantSquare == -1)
                ? " -"
                : (" " + std::string(1, 'a' + file_of(enPassantSquare)) +
                   std::string(1, '1' + rank_of(enPassantSquare)));
  result += " " + std::to_string(halfMove) + " " + std::to_string(fullMove);
  return result;
}

namespace {
const int DIAG_DIRS[4] = {9, 7, -7, -9};
const int ORTHO_DIRS[4] = {8, 1, -1, -8};

Bitboard ray_attacks(Square square, const int *dirs, int n_dirs,
                     uint64_t bitboard) {
  Bitboard attacks = 0;
  for (int d = 0; d < n_dirs; d++) {
    int cur = square + dirs[d];
    while (cur >= 0 && cur < 64 &&
           std::abs(file_of(cur) - file_of(cur - dirs[d])) <= 1) {
      attacks |= 1ULL << cur;
      if (bitboard & (1ULL << cur))
        break;
      cur += dirs[d];
    }
  }
  return attacks;
}

Bitboard knight_attacks(Square square) {
  static const int offsets[] = {17, 15, 10, 6, -6, -10, -15, -17};
  Bitboard result = 0;
  for (int off : offsets) {
    int t = square + off;
    if (t >= 0 && t < 64 && std::abs(file_of(t) - file_of(square)) <= 2)
      result |= 1ULL << t;
  }
  return result;
}

Bitboard king_attacks(Square square) {
  static const int offsets[] = {9, 8, 7, 1, -1, -7, -8, -9};
  Bitboard result = 0;
  for (int off : offsets) {
    int t = square + off;
    if (t >= 0 && t < 64 && std::abs(file_of(t) - file_of(square)) <= 1)
      result |= 1ULL << t;
  }
  return result;
}
} // namespace

bool Board::is_attacked(Square square, Colour colour) const {
  Bitboard bitboard = bitboard_all();

  // Pawns
  Bitboard pawns = bitboards[make_piece(colour, PAWN).index()];
  if (colour == WHITE) {
    if (((pawns << 7) & ~0x8080808080808080ULL) & (1ULL << square))
      return true;
    if (((pawns << 9) & ~0x0101010101010101ULL) & (1ULL << square))
      return true;
  } else {
    if (((pawns >> 7) & ~0x0101010101010101ULL) & (1ULL << square))
      return true;
    if (((pawns >> 9) & ~0x8080808080808080ULL) & (1ULL << square))
      return true;
  }

  if (knight_attacks(square) & bitboards[make_piece(colour, KNIGHT).index()])
    return true;
  if (king_attacks(square) & bitboards[make_piece(colour, KING).index()])
    return true;

  Bitboard bishopsQueens = bitboards[make_piece(colour, BISHOP).index()] |
                           bitboards[make_piece(colour, QUEEN).index()];
  if (ray_attacks(square, DIAG_DIRS, 4, bitboard) & bishopsQueens)
    return true;

  Bitboard rooksQueens = bitboards[make_piece(colour, ROOK).index()] |
                         bitboards[make_piece(colour, QUEEN).index()];
  if (ray_attacks(square, ORTHO_DIRS, 4, bitboard) & rooksQueens)
    return true;

  return false;
}

bool Board::is_in_check() const {
  Bitboard king_bb = bitboards[make_piece(sideToMove, KING).index()];
  if (!king_bb)
    return false;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  return is_attacked(ksq, flip(sideToMove));
}

void Board::update_castling_rights(Square from, Square to) {
  constexpr CastlingRights castling_mask[64] = {
      static_cast<CastlingRights>(13), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(12), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(14),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(7),  static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(3),  static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(11),
  };
  hash ^= Zobrist::castlingRights[castlingRights];
  castlingRights &= castling_mask[from] & castling_mask[to];
  hash ^= Zobrist::castlingRights[castlingRights];
}

UndoInfo Board::make_move(Move move) {
  UndoInfo undoInfo{move,           squares[move.to], enPassantSquare,
                    castlingRights, halfMove,         hash};

  if (enPassantSquare != -1) {
    hash ^= Zobrist::enPassantFile[file_of(enPassantSquare)];
    enPassantSquare = -1;
  }

  if (move.type == CASTLE) {
    // Move king
    move_piece(move.from, move.to);
    // Move rook
    bool kingside = move.to > move.from;
    Square rook_from = to_square(kingside ? 7 : 0, rank_of(move.from));
    Square rook_to = to_square(kingside ? 5 : 3, rank_of(move.from));
    move_piece(rook_from, rook_to);
  } else {
    // Capture
    if (squares[move.to].is_empty()) {
      remove_piece(move.to);
      halfMove = 0;
    }
    // En passant capture
    if (move.type == EN_PASSANT) {
      Square ep_cap = to_square(file_of(move.to), rank_of(move.from));
      remove_piece(ep_cap);
      halfMove = 0;
    }
    move_piece(move.from, move.to);

    if ((squares[move.to]).type == PAWN) {
      halfMove = 0;
      // Set new en passant square for double pawn push
      int diff = move.to - move.from;
      if (diff == 16 || diff == -16) {
        enPassantSquare = move.from + diff / 2;
        hash ^= Zobrist::enPassantFile[file_of(enPassantSquare)];
      }
    }
    // Promotion
    if (move.type == PROMOTE) {
      remove_piece(move.to);
      put_piece(make_piece(sideToMove, move.promote_pt), move.to);
    }
  }

  update_castling_rights(move.from, move.to);
  halfMove++;

  sideToMove = flip(sideToMove);
  hash ^= Zobrist::side;
  if (sideToMove == WHITE)
    fullMove++;

  return undoInfo;
}

void Board::undo_move(Move move, const UndoInfo &undoInfo) {
  sideToMove = flip(sideToMove);
  if (sideToMove == BLACK)
    fullMove--;

  if (move.type == CASTLE) {
    move_piece(move.to, move.from);
    bool kingside = move.to > move.from;
    Square rook_to = to_square(kingside ? 5 : 3, rank_of(move.from));
    Square rook_from = to_square(kingside ? 7 : 0, rank_of(move.from));
    move_piece(rook_to, rook_from);
  } else {
    if (move.type == PROMOTE) {
      remove_piece(move.to);
      put_piece(make_piece(sideToMove, PAWN), move.to);
    } else {
      move_piece(move.to, move.from);
    }
    if (undoInfo.captured.is_empty())
      put_piece(undoInfo.captured, move.to);
    if (move.type == EN_PASSANT) {
      Square ep_cap = to_square(file_of(move.to), rank_of(move.from));
      put_piece(make_piece(flip(sideToMove), PAWN), ep_cap);
    }
  }

  enPassantSquare = undoInfo.enPassantSquare;
  castlingRights = undoInfo.castlingRights;
  halfMove = undoInfo.halfMove;
  hash = undoInfo.hash;
}

UndoInfo Board::make_drop(PieceType pt, Square to) {
  UndoInfo undoInfo{Move::drop(pt, to), Piece{},  enPassantSquare,
                    castlingRights,     halfMove, hash};
  if (enPassantSquare != -1) {
    hash ^= Zobrist::enPassantFile[file_of(enPassantSquare)];
    enPassantSquare = -1;
  }
  put_piece(make_piece(sideToMove, pt), to);
  // drops reset 50-move clock
  halfMove = 0;
  sideToMove = flip(sideToMove);
  hash ^= Zobrist::side;
  if (sideToMove == WHITE)
    fullMove++;
  return undoInfo;
}

void Board::undo_drop(Square to, const UndoInfo &undoInfo) {
  sideToMove = flip(sideToMove);
  if (sideToMove == BLACK)
    fullMove--;
  remove_piece(to);
  enPassantSquare = undoInfo.enPassantSquare;
  halfMove = undoInfo.halfMove;
  hash = undoInfo.hash;
}

bool Board::is_legal(Move move) const {
  Board copy = *this;
  Colour moved_side = sideToMove;

  if (move.is_drop()) {
    copy.make_drop(move.drop_pt, move.to);
  } else {
    copy.make_move(move);
  }

  Bitboard king_bb = copy.bitboards[make_piece(moved_side, KING).index()];
  if (!king_bb) {
    return false;
  }
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  return !copy.is_attacked(ksq, copy.sideToMove);
}

bool Board::is_checkmate() const {
  if (!is_in_check())
    return false;
  auto moves = generate_moves(*this);
  for (auto m : moves)
    if (is_legal(m))
      return false;
  return true;
}

bool Board::is_stalemate() const {
  if (is_in_check())
    return false;
  auto moves = generate_moves(*this);
  for (auto m : moves)
    if (is_legal(m))
      return false;
  return true;
}

void Board::print() const {
  std::cout << "\n  a b c d e f g h\n";
  for (int rank = 7; rank >= 0; rank--) {
    std::cout << (rank + 1) << ' ';
    for (int file = 0; file < 8; file++) {
      Piece piece = squares[to_square(file, rank)];
      std::cout << piece.to_char() << ' ';
    }
    std::cout << (rank + 1) << '\n';
  }
  std::cout << "  a b c d e f g h\n\n";
  std::cout << "FEN: " << to_fen() << '\n';
}

void Board::init_zobrist() { Zobrist::ensure_init(); }