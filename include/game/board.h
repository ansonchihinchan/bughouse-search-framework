#pragma once

#include "game/types.h"
#include <array>
#include <cstdint>
#include <string>

constexpr int SQUARE_NO = 64;
constexpr int PIECE_NO =
    static_cast<int>(PIECE_TYPE_NO) * static_cast<int>(COLOUR_NO);

using Bitboard = uint64_t;

struct BoardUndo {
  Piece captured;

  Square enPassantSquare = -1;
  CastlingRights castlingRights = ANY_CASTLING;

  int halfMove = 0;
  uint64_t hash = 0;
};

class Board {
public:
  std::array<Piece, SQUARE_NO> squares{};

  std::array<Bitboard, PIECE_NO> bitboards{};
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
    return bitboards[piece.index()];
  }

  // Return bitboard of all pieces of one colour
  Bitboard bitboard_colour(Colour colour) const;

  // Return bitboard of all pieces
  Bitboard bitboard_all() const;

  BoardUndo make_move(Move move);
  void undo_move(Move move, const BoardUndo &undo);

  // Place piece on square 'to' for side_to_move
  BoardUndo make_drop(PieceType pt, Square to);
  void undo_drop(Square to, const BoardUndo &undo);

  BoardUndo make_null_move();
  void undo_null_move(const BoardUndo &undo);

  bool is_in_check() const;
  bool is_attacked(Square square, Colour colour) const;
  bool is_legal(Move move) const;
  // Fast king-safety check for pseudo-legal movegen
  bool is_king_safe_after(Move move) const;
  bool is_capture(Move move) const {
    return move.type == EN_PASSANT || !piece_on(move.to).is_empty();
  }
  bool has_non_pawn(Colour colour) const;

  // Only for testing
  bool is_checkmate() const;
  bool is_stalemate() const;

  void print() const;

  static void init_zobrist();

  bool operator==(const Board &other) const = default;

private:
  void put_piece(Piece piece, Square square);
  void remove_piece(Square square);
  void move_piece(Square from, Square to);
  void update_castling_rights(Square from, Square to);
};