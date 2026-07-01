#pragma once

#include "board.h"
#include "clock.h"
#include "pocket.h"
#include "types.h"
#include <array>

#define PLAYER_NO 4

enum class GameResult { ONGOING, WHITE_WINS, BLACK_WINS, DRAW };

struct BughouseState {
  std::array<Board, COLOUR_NO> boards;
  std::array<Pocket, PLAYER_NO> pockets;
  BughouseClock clock;

  BughouseState();
  void reset();

  // Returns false if move is illegal
  bool apply_move(int player_id, Move m);

  // Board 0: (0, 1)
  // Board 1: (2, 3)
  int board_of(int player_id) const { return player_id < 2 ? 0 : 1; }

  // Partners: (0, 2), (1, 3)
  int partner_of(int player_id) const { return player_id ^ 2; }

  GameResult result() const;

  void print() const;
};