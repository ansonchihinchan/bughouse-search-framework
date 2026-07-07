#pragma once

#include <assert.h>
#include <cstdint>
#include <string>

enum Colour : uint8_t { WHITE, BLACK, COLOUR_NO = 2 };
constexpr Colour flip(Colour colour) { return static_cast<Colour>(colour ^ 1); }

using Square = int; // a1 = 0, ..., h8 = 63

constexpr int rank_of(Square square) { return square >> 3; }
constexpr int file_of(Square square) { return square & 7; }
constexpr Square to_square(int file, int rank) { return rank * 8 + file; }

enum PieceType : uint8_t {
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
  PieceType type = NO_PIECE_TYPE;
  Colour colour = WHITE;

  constexpr bool is_empty() const { return type == NO_PIECE_TYPE; }

  constexpr bool operator==(const Piece &other) const {
    return type == other.type && colour == other.colour;
  }

  constexpr bool operator!=(const Piece &other) const {
    return !(*this == other);
  }

  constexpr int index() const {
    assert(!is_empty());
    return static_cast<int>(colour) * (PIECE_TYPE_NO - 1) +
           static_cast<int>(type) - 1;
  }

  constexpr PieceType type_of(const Piece &p) { return p.type; }

  constexpr Colour colour_of(const Piece &p) { return p.colour; }

  constexpr char to_char() const {
    constexpr char piece_chars[COLOUR_NO][PIECE_TYPE_NO] = {
        {' ', 'P', 'N', 'B', 'R', 'Q', 'K'},
        {' ', 'p', 'n', 'b', 'r', 'q', 'k'}};

    return piece_chars[static_cast<int>(colour)][static_cast<int>(type)];
  }
};

constexpr Piece make_piece(Colour c, PieceType pt) { return Piece{pt, c}; }

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

inline CastlingRights operator|(CastlingRights a, CastlingRights b) {
  return static_cast<CastlingRights>(static_cast<uint8_t>(a) |
                                     static_cast<uint8_t>(b));
}

inline CastlingRights &operator|=(CastlingRights &a, CastlingRights b) {
  a = a | b;
  return a;
}

inline CastlingRights operator&(CastlingRights a, CastlingRights b) {
  return static_cast<CastlingRights>(static_cast<uint8_t>(a) &
                                     static_cast<uint8_t>(b));
}

inline CastlingRights &operator&=(CastlingRights &a, CastlingRights b) {
  a = a & b;
  return a;
}

inline std::string square_to_str(Square square) {
  return std::string() + char('a' + (square % 8)) + char('1' + (square / 8));
}

struct Move {
  Square from = -1;
  Square to = -1;
  MoveType type = NORMAL;
  PieceType promote_pt = NO_PIECE_TYPE;
  PieceType drop_pt = NO_PIECE_TYPE;

  // Constructors
  static Move normal(Square from, Square to) {
    return {from, to, NORMAL, NO_PIECE_TYPE, NO_PIECE_TYPE};
  }
  static Move promote(Square from, Square to, PieceType promote_pt) {
    return {from, to, PROMOTE, promote_pt, NO_PIECE_TYPE};
  }
  static Move en_passant(Square from, Square to) {
    return {from, to, EN_PASSANT, NO_PIECE_TYPE, NO_PIECE_TYPE};
  }
  static Move castling(Square from, Square to) {
    return {from, to, CASTLE, NO_PIECE_TYPE, NO_PIECE_TYPE};
  }
  static Move drop(PieceType drop_pt, Square to) {
    return {-1, to, DROP, NO_PIECE_TYPE, drop_pt};
  }

  bool is_drop() const { return type == DROP; }
  bool is_none() const { return from == -1 && type != DROP; }

  bool operator==(const Move &move) const {
    return from == move.from && to == move.to && type == move.type &&
           promote_pt == move.promote_pt && drop_pt == move.drop_pt;
  }
  bool operator!=(const Move &move) const { return !(*this == move); }

  std::string to_string() const {
    if (type == DROP) {
      // e.g. "N@e4"
      constexpr char pt_chars[] = " PNBRQK";
      return std::string(1, pt_chars[drop_pt]) + "@" + square_to_str(to);
    }

    // e.g. "e2e4"
    std::string s = square_to_str(from) + square_to_str(to);

    // e.g. "e7e8q"
    if (type == PROMOTE) {
      constexpr char pt_chars[] = " pnbrqk";
      s += pt_chars[promote_pt];
    }

    return s;
  }
};

enum class PlayerId : int {};

constexpr int to_int(PlayerId p) { return static_cast<int>(p); }
constexpr PlayerId to_player(int i) { return static_cast<PlayerId>(i); }

constexpr PlayerId operator^(PlayerId p, int x) {
  return to_player(to_int(p) ^ x);
}

constexpr bool operator==(PlayerId p, int i) { return to_int(p) == i; }
constexpr bool operator==(int i, PlayerId p) { return i == to_int(p); }
constexpr bool operator!=(PlayerId p, int i) { return !(p == i); }
constexpr bool operator!=(int i, PlayerId p) { return !(i == p); }

constexpr PlayerId NO_PLAYER = to_player(-1);

constexpr int PLAYER_NO = 4;
constexpr int BOARD_NO = 2;