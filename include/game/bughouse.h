#pragma once

#include "board.h"
#include "clock.h"
#include "pocket.h"
#include "types.h"
#include <array>
#include <optional>

enum class GameResult { ONGOING, TEAM_A_WINS, TEAM_B_WINS, DRAW };

struct BughouseUndo {
  BoardUndo board;

  PieceType removedFromPocket = NO_PIECE_TYPE;

  bool creditedPartner = false;
  PieceType creditedPiece = NO_PIECE_TYPE;
};

struct BughousePosition {
  std::array<Board, BOARD_NO> boards;
  std::array<Pocket, PLAYER_NO> pockets;
};

// Board 0: (0, 1)
// Board 1: (2, 3)
inline int board_of(PlayerId player) { return to_int(player) < 2 ? 0 : 1; }

// Partners: (0, 2), (1, 3)
inline PlayerId partner_of(PlayerId player) { return player ^ 2; }

inline Colour colour_of_player(PlayerId player) {
  int p = to_int(player);
  assert(p >= 0 && p < PLAYER_NO);
  return (p == 0 || p == 3) ? WHITE : BLACK;
}

inline PlayerId next_player(PlayerId player) {
  assert(to_int(player) < PLAYER_NO);
  return player ^ 1;
}

struct BughouseState {
  BughousePosition position;
  BughouseClock clock;

  BughouseState();
  void reset();

  GameResult result() const;

  void print() const;
};

BughouseUndo apply_move(BughousePosition &position, PlayerId player, Move move);

void undo_move(BughousePosition &position, PlayerId player, Move move,
               const BughouseUndo &undo);

bool is_checkmate(const BughousePosition &position, PlayerId player);
bool is_stalemate(const BughousePosition &position, PlayerId player);