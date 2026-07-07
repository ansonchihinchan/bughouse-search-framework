#include "game/bughouse.h"
#include "game/movegen.h"
#include <cassert>
#include <iostream>

// Default time control: 3 + 2
#define DEFAULT_TIME 3 * 60 * 1000
#define DEFAULT_INCREMENT 2 * 1000

// Player 0 = White on board 0.  Captures go to player 2's reserve.
// Player 1 = Black on board 0.  Captures go to player 3's reserve.
// Player 2 = Black on board 1.  Captures go to player 0's reserve.
// Player 3 = White on board 1.  Captures go to player 1's reserve.

BughouseState::BughouseState() { reset(); }

void BughouseState::reset() {
  for (auto &b : position.boards)
    b.reset();
  for (auto &p : position.pockets)
    p = Pocket{};
  clock.set(DEFAULT_TIME, DEFAULT_INCREMENT);
}

BughouseUndo apply_move(BughousePosition &position, PlayerId player,
                        Move move) {
  assert(position.boards[board_of(player)].is_legal(move));

  int board_idx = board_of(player);
  Board &board = position.boards[board_idx];
  Colour player_colour = colour_of_player(player);

  PlayerId partner = partner_of(player);
  Pocket &player_pocket = position.pockets[to_int(player)];
  Pocket &partner_pocket = position.pockets[to_int(partner)];

  BughouseUndo undo{};

  if (move.is_drop()) {
    undo.removedFromPocket = move.drop_pt;

    player_pocket.remove(move.drop_pt);
    undo.board = board.make_drop(move.drop_pt, move.to);
  } else {
    Piece captured = board.piece_on(move.to);
    if (move.type == EN_PASSANT)
      captured = make_piece(flip(board.sideToMove), PAWN);

    undo.board = board.make_move(move);

    // Transfer capture to partner's reserve
    if (!captured.is_empty() && captured.type != KING) {
      partner_pocket.add(captured.type);
      undo.creditedPartner = true;
      undo.creditedPiece = captured.type;
    }
  }

  return undo;
}

void undo_move(BughousePosition &position, PlayerId player, Move move,
               const BughouseUndo &undo) {

  Board &board = position.boards[board_of(player)];

  if (undo.creditedPartner) {
    PlayerId partner = partner_of(player);
    position.pockets[to_int(partner)].remove(undo.creditedPiece);
  }

  if (move.is_drop()) {
    board.undo_drop(move.to, undo.board);
    position.pockets[to_int(player)].add(undo.removedFromPocket);
  } else {
    board.undo_move(move, undo.board);
  }
}

GameResult BughouseState::result() const {
  if (clock.any_flagged()) {
    for (int i = 0; i < PLAYER_NO; i++) {
      if (clock.flagged(to_player(i))) {
        return (i == 0 || i == 2) ? GameResult::TEAM_B_WINS
                                  : GameResult::TEAM_A_WINS;
      }
    }
  }
  for (int b = 0; b < BOARD_NO; b++) {
    if (position.boards[b].is_checkmate()) {
      Colour loser = position.boards[b].sideToMove;
      int player_id = (b == 0) ? loser : (3 - loser);
      return (player_id == 0 || player_id == 2) ? GameResult::TEAM_B_WINS
                                                : GameResult::TEAM_A_WINS;
    }
  }
  return GameResult::ONGOING;
}

bool is_checkmate(const BughousePosition &position, PlayerId player) {

  const Board &board = position.boards[board_of(player)];

  if (!board.is_in_check())
    return false;

  auto moves = generate_legal_moves(position, player);

  return moves.empty();
}

bool is_stalemate(const BughousePosition &position, PlayerId player) {

  const Board &board = position.boards[board_of(player)];

  if (board.is_in_check())
    return false;

  auto moves = generate_legal_moves(position, player);

  return moves.empty();
}

void BughouseState::print() const {
  std::cout << "=== Board A (players 0=W, 1=B) ===\n";
  position.boards[0].print();
  std::cout << "Pocket 0 (White): ";
  position.pockets[0].print();
  std::cout << "Pocket 1 (Black): ";
  position.pockets[1].print();

  std::cout << "\n=== Board B (players 3=W, 2=B) ===\n";
  position.boards[1].print();
  std::cout << "Pocket 3 (White): ";
  position.pockets[3].print();
  std::cout << "Pocket 2 (Black): ";
  position.pockets[2].print();
}