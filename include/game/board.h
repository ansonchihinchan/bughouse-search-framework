#pragma once
#include "types.h"
#include <array>
#include <cstdint>
#include <string>

#define SQUARE_NO 64
#define PIECE_NO (PIECE_TYPE_NO * COLOUR_NO)

using Bitboard = uint64_t;

struct UndoInfo {
  Move move;
  Piece captured;

  Square enPassantSquare = -1;
  uint8_t castlingRights = ANY_CASTLING;

  int halfmoveClock = 0;
  uint64_t hash = 0;

  PieceType droppedPiece = NO_PIECE_TYPE;
  PieceType pocketGain = NO_PIECE_TYPE;
};

class Board {
public:
  std::array<Piece, SQUARE_NO> squares{};

  Bitboard bitboards[COLOUR_NO][PIECE_TYPE_NO]{};
  Colour sideToMove = WHITE;
  Square enPassantSquare = -1;
  CastlingRights castlingRights = ANY_CASTLING;

  // 50-move rule counter
  int halfMove = 0;

  int fullMove = 1;

  // Zobrist hash
  uint64_t hash = 0;

  // Constructor
  Board();
  explicit Board(const std::string &fen);

  void reset();

  // FEN
  bool load_fen(const std::string &fen);
  std::string to_fen() const;

  Piece piece_on(Square square) const { return squares[square]; }
  bool is_empty(Square square) const {
    return squares[square].type == NO_PIECE_TYPE;
  }

  // Return bitboard of one piece
  Bitboard bitboard_piece(Piece piece) const {
    return bitboards[piece.colour][piece.type];
  }

  // Return bitboard of all pieces of one colour
  Bitboard bitboard_colour(Colour colour) const;

  // Return bitboard of all pieces
  Bitboard bitboard_all() const;

  UndoInfo make_move(Move move);
  void undo_move(Move move, const UndoInfo &undoInfo);

  // Place piece on square 'to' for side_to_move
  UndoInfo make_drop(PieceType pt, Square to);
  void undo_drop(PieceType pt, Square to, const UndoInfo &undoInfo);

  bool is_in_check() const;
  bool is_attacked(Square square, Colour colour) const;
  bool is_legal(Move move) const;

  bool is_checkmate() const;
  bool is_stalemate() const;

  void print() const;

  static void init_zobrist();

private:
  void put_piece(Piece piece, Square square);
  void remove_piece(Square square);
  void move_piece(Square from, Square to);
  void update_castling_rights(Square from, Square to);
};