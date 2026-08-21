#include "game/board.h"
#include "game/attacks.h"
#include "game/bitboards.h"
#include "game/movegen.h"
#include "game/zobrist.h"
#include <bit>
#include <cassert>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>

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

bool Board::is_attacked(Square square, Colour colour) const {
  Bitboard bitboard = bitboard_all();

  // Pawns
  Bitboard pawns = bitboards[make_piece(colour, PAWN).index()];
  if (pawn_attacks(pawns, colour) & (1ULL << square))
    return true;

  if (knight_attacks(square) & bitboards[make_piece(colour, KNIGHT).index()])
    return true;
  if (king_attacks(square) & bitboards[make_piece(colour, KING).index()])
    return true;

  Bitboard bishopsQueens = bitboards[make_piece(colour, BISHOP).index()] |
                           bitboards[make_piece(colour, QUEEN).index()];
  if (bishop_attacks(square, bitboard) & bishopsQueens)
    return true;

  Bitboard rooksQueens = bitboards[make_piece(colour, ROOK).index()] |
                         bitboards[make_piece(colour, QUEEN).index()];
  if (rook_attacks(square, bitboard) & rooksQueens)
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
      static_cast<CastlingRights>(7),  static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(3),  static_cast<CastlingRights>(15),
      static_cast<CastlingRights>(15), static_cast<CastlingRights>(11),
  };
  hash ^= Zobrist::castlingRights[castlingRights];
  castlingRights &= castling_mask[from] & castling_mask[to];
  hash ^= Zobrist::castlingRights[castlingRights];
}

BoardUndo Board::make_move(Move move) {
  BoardUndo undo{squares[move.to], enPassantSquare, castlingRights, halfMove,
                 hash};

  Piece movedPiece = squares[move.from];

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
    if (!squares[move.to].is_empty()) {
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

  if (movedPiece.type == PAWN || !undo.captured.is_empty() ||
      move.type == EN_PASSANT)
    halfMove = 0;
  else
    halfMove++;

  sideToMove = flip(sideToMove);
  hash ^= Zobrist::side;
  if (sideToMove == WHITE)
    fullMove++;

  return undo;
}

void Board::undo_move(Move move, const BoardUndo &undo) {
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
      put_piece(make_piece(sideToMove, PAWN), move.from);
    } else {
      move_piece(move.to, move.from);
    }
    if (!undo.captured.is_empty())
      put_piece(undo.captured, move.to);
    if (move.type == EN_PASSANT) {
      Square ep_cap = to_square(file_of(move.to), rank_of(move.from));
      put_piece(make_piece(flip(sideToMove), PAWN), ep_cap);
    }
  }

  enPassantSquare = undo.enPassantSquare;
  castlingRights = undo.castlingRights;
  halfMove = undo.halfMove;
  hash = undo.hash;
}

BoardUndo Board::make_drop(PieceType pt, Square to) {
  BoardUndo undo{Piece{}, enPassantSquare, castlingRights, halfMove, hash};
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
  return undo;
}

void Board::undo_drop(Square to, const BoardUndo &undo) {
  sideToMove = flip(sideToMove);
  if (sideToMove == BLACK)
    fullMove--;
  remove_piece(to);
  enPassantSquare = undo.enPassantSquare;
  halfMove = undo.halfMove;
  hash = undo.hash;
}

BoardUndo Board::make_null_move() {
  BoardUndo undo{Piece{}, enPassantSquare, castlingRights, halfMove, hash};

  if (enPassantSquare != -1) {
    hash ^= Zobrist::enPassantFile[file_of(enPassantSquare)];
    enPassantSquare = -1;
  }

  sideToMove = flip(sideToMove);
  hash ^= Zobrist::side;

  if (sideToMove == WHITE)
    fullMove++;
  return undo;
}

void Board::undo_null_move(const BoardUndo &undo) {
  sideToMove = flip(sideToMove);

  if (sideToMove == BLACK)
    fullMove--;

  enPassantSquare = undo.enPassantSquare;
  castlingRights = undo.castlingRights;
  halfMove = undo.halfMove;
  hash = undo.hash;
}

bool Board::has_non_pawn(Colour colour) const {
  for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN})
    if (bitboards[make_piece(colour, pt).index()])
      return true;
  return false;
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
  auto moves = generate_pseudo_legal_moves(*this);
  for (auto m : moves)
    if (is_legal(m))
      return false;
  return true;
}

bool Board::is_stalemate() const {
  if (is_in_check())
    return false;
  auto moves = generate_pseudo_legal_moves(*this);
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