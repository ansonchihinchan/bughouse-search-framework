#include "bughouse.h"
#include "movegen.h"
#include <iostream>

// Default time control: 3 + 2
#define DEFAULT_TIME 3 * 60 * 1000
#define DEFAULT_INCREMENT 2 * 1000

// Player 0 = White on board 0.  Captures go to player 2's reserve.
// Player 1 = Black on board 0.  Captures go to player 3's reserve.
// Player 2 = Black on board 1.  Captures go to player 0's reserve.
// Player 3 = White on board 1.  Captures go to player 1's reserve.

static Colour colour_of_player(int player_id) {
  assert(player_id < PLAYER_NO);
  return (player_id == 0 || player_id == 3) ? WHITE : BLACK;
}

static int next_player(int player_id) {
  assert(player_id < PLAYER_NO);
  return player_id ^ 1;
}

BughouseState::BughouseState() { reset(); }

void BughouseState::reset() {
  for (auto &b : boards)
    b.reset();
  for (auto &p : pockets)
    p = Pocket{};
  clock.set(DEFAULT_TIME, DEFAULT_INCREMENT);
}

bool BughouseState::apply_move(int player_id, Move move) {
  assert(player_id < PLAYER_NO);
  int board_idx = board_of(player_id);
  Board &board = boards[board_idx];
  Colour player_colour = colour_of_player(player_id);

  if (board.sideToMove != player_colour)
    return false;

  Pocket &player_pocket = pockets[player_id];
  Pocket &partner_pocket = pockets[partner_of(player_id)];

  if (move.is_drop()) {
    if (!player_pocket.contains(move.drop_pt))
      return false;

    // Validate the drop doesn't leave own king in check
    Board copy = board;
    copy.make_drop(move.drop_pt, move.to);
    Square ksq = static_cast<Square>(__builtin_ctzll(
        copy.bitboards[make_piece(player_colour, KING).index()]));
    if (copy.is_attacked(ksq, flip(board.sideToMove)))
      return false;

    board.make_drop(move.drop_pt, move.to);
    player_pocket.remove(move.drop_pt);
  } else {
    if (!board.is_legal(move))
      return false;

    Piece captured = board.piece_on(move.to);
    if (move.type == EN_PASSANT)
      captured = make_piece(flip(board.sideToMove), PAWN);

    board.make_move(move);

    // Transfer capture to partner's reserve
    if (!captured.is_empty()) {
      PieceType cap_type = captured.type;
      if (cap_type != KING)
        partner_pocket.add(cap_type);
    }
  }

  clock.stop(player_id);
  clock.start(next_player(player_id));
  return true;
}

GameResult BughouseState::result() const {
  if (clock.any_flagged()) {
    for (int i = 0; i < PLAYER_NO; i++) {
      if (clock.flagged(i)) {
        return (i == 0 || i == 3) ? GameResult::BLACK_WINS
                                  : GameResult::WHITE_WINS;
      }
    }
  }
  for (int b = 0; b < BOARD_NO; b++) {
    if (boards[b].is_checkmate()) {
      Colour loser = boards[b].sideToMove;
      return (loser == WHITE) ? GameResult::BLACK_WINS : GameResult::WHITE_WINS;
    }
  }
  return GameResult::ONGOING;
}

void BughouseState::print() const {
  std::cout << "=== Board 0 (players 0=W, 1=B) ===\n";
  boards[0].print();
  std::cout << "Pocket 0 (White): ";
  pockets[0].print();
  std::cout << "Pocket 1 (Black): ";
  pockets[1].print();

  std::cout << "\n=== Board 1 (players 3=W, 2=B) ===\n";
  boards[1].print();
  std::cout << "Pocket 3 (White): ";
  pockets[3].print();
  std::cout << "Pocket 2 (Black): ";
  pockets[2].print();
}