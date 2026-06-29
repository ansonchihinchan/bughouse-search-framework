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

enum CastlingRights { NO_CASTLING, WHITE_OO, WHITE_OOO, BLACK_OO, BLACK_OOO };

struct Move {
  Square from;
  Square to;
  MoveType type = NORMAL;
  PieceType promotion = NO_PIECE_TYPE;
};