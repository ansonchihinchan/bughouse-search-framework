#pragma once
#include "types.h"
#include <cstdint>

#define SQUARE_NO 64

class Board {
public:
  Piece squares[SQUARE_NO];
  Colour sideToMove = WHITE;
  Square enPassantSquare = -1;
  CastlingRights castlingRights = ANY_CASTLING;
};

class Position {
public:
  Board board;
  Colour sideToMove;
  CastlingRights castlingRights;
  Square enPassantSquare = -1;
};