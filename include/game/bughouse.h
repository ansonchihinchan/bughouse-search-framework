#pragma once

#include "game/board.h"
#include "game/clock.h"
#include "game/pocket.h"
#include "game/types.h"
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

inline PlayerId player_on_board(int board_idx, Colour colour) {
  if (board_idx == 0) {
    return to_player(colour == WHITE ? 0 : 1);
  } else {
    return to_player(colour == WHITE ? 3 : 2);
  }
}

// Colour of player's team on board board_idx
inline Colour team_colour(PlayerId player, int board_idx) {
  return colour_of_player((board_of(player) == board_idx) ? player
                                                          : partner_of(player));
}

// Returns +1 if player 2 is on player1's team and -1 otherwise
inline int team_sign(PlayerId player1, PlayerId player2) {
  return (player2 == player1 || player2 == partner_of(player1)) ? 1 : -1;
}

struct BughouseState {
  BughousePosition position;
  BughouseClock clock;

  BughouseState();
  void reset();

  GameResult result() const;

  void print() const;
};

uint64_t position_hash(const BughousePosition &position);

BughouseUndo apply_move(BughousePosition &position, PlayerId player, Move move);

void undo_move(BughousePosition &position, PlayerId player, Move move,
               const BughouseUndo &undo);

BoardUndo make_null_move(BughousePosition &position, PlayerId player);
void undo_null_move(BughousePosition &position, PlayerId player,
                    const BoardUndo &undo);

bool is_checkmate(const BughousePosition &position, PlayerId player);
bool is_stalemate(const BughousePosition &position, PlayerId player);