#pragma once

#include <cstdint>

enum Colour : uint8_t { WHITE, BLACK, COLOUR_NO = 2 };

enum PieceType {
  NO_PIECE_TYPE,
  PAWN,
  KNIGHT,
  BISHOP,
  ROOK,
  QUEEN,
  KING,
  PIECE_TYPE_NO = 7
};

struct Piece {
  PieceType type;
  Colour colour;
};

using Square = int; // a1 = 0, ..., h8 = 63

enum MoveType { NORMAL, PROMOTE, EN_PASSANT, CASTLE, DROP };

enum CastlingRights : uint8_t {
  NO_CASTLING = 0,
  WHITE_OO = 1 << 0,  // 0001
  WHITE_OOO = 1 << 1, // 0010
  BLACK_OO = 1 << 2,  // 0100
  BLACK_OOO = 1 << 3, // 1000

  WHITE_CASTLING = WHITE_OO | WHITE_OOO,
  BLACK_CASTLING = BLACK_OO | BLACK_OOO,

  ANY_CASTLING = WHITE_CASTLING | BLACK_CASTLING
};

struct Move {
  Square from;
  Square to;
  MoveType type = NORMAL;
  PieceType promotion = NO_PIECE_TYPE;
};