#include "game/bughouse.h"
#include "game/movegen.h"
#include "game/zobrist.h"
#include <algorithm>
#include <cassert>
#include <iostream>

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

  history.clear();
  history.push_back(RepetitionNode{position_hash(position), 0, 0});
}

uint64_t position_hash(const BughousePosition &position) {
  Zobrist::ensure_init();

  uint64_t hash = position.boards[0].hash;
  hash ^= (position.boards[1].hash << 1) | (position.boards[1].hash >> 63);

  for (int p = 0; p < PLAYER_NO; p++) {
    const Pocket &pocket = position.pockets[p];
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      int count = pocket.count(static_cast<PieceType>(pt));
      if (count > 0)
        hash ^= Zobrist::pocket[p][pt]
                               [std::min(count, Zobrist::MAX_POCKET_COUNT - 1)];
    }
  }
  return hash;
}

// Assumes move is already validated
BughouseUndo apply_move(BughousePosition &position, PlayerId player,
                        Move move) {
  assert(position.boards[board_of(player)].is_legal(move));

  int board_idx = board_of(player);
  Board &board = position.boards[board_idx];

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

BoardUndo make_null_move(BughousePosition &position, PlayerId player) {
  return position.boards[board_of(player)].make_null_move();
}

void undo_null_move(BughousePosition &position, PlayerId player,
                    const BoardUndo &undo) {
  position.boards[board_of(player)].undo_null_move(undo);
}

BughouseUndo BughouseState::make_move(PlayerId player, Move move) {
  const Board &board = position.boards[board_of(player)];

  Piece moved_piece = move.is_drop() ? Piece{} : board.piece_on(move.from);
  bool irreversible = move.is_drop() || move.type == PROMOTE ||
                      move.type == EN_PASSANT || moved_piece.type == PAWN ||
                      board.is_capture(move);

  BughouseUndo undo = apply_move(position, player, move);

  clock.stop(player);
  clock.start(next_player(player));

  int prev_reversible = history.empty() ? 0 : history.back().reversible_plies;
  int reversible_plies = irreversible ? 0 : prev_reversible + 1;

  uint64_t key = position_hash(position);
  int repetition = mark_repetition(history, key, reversible_plies);

  history.push_back(RepetitionNode{key, reversible_plies, repetition});

  return undo;
}

void BughouseState::unmake_move(PlayerId player, Move move,
                                const BughouseUndo &undo) {
  undo_move(position, player, move, undo);
  if (!history.empty())
    history.pop_back();
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
    PlayerId player = player_on_board(b, position.boards[b].sideToMove);
    if (is_checkmate(position, player)) {
      int player_id = to_int(player);
      return (player_id == 0 || player_id == 2) ? GameResult::TEAM_B_WINS
                                                : GameResult::TEAM_A_WINS;
    }
  }
  if (!history.empty() && history.back().repetition < 0)
    return GameResult::DRAW;
  for (const Board &board : position.boards)
    if (board.halfMove >= HALFMOVE_LIMIT)
      return GameResult::DRAW;
  // Stalemate is not treated as a loss or draw: the stalemated side simply has
  // no board move available this instant. Their own clock keeps running until
  // their partner captures something on the other board crediting a piece to
  // this player's pocket which may give them a legal drop and unfreeze the
  // board.
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

bool is_legal_move(const BughousePosition &position, PlayerId player,
                   Move move) {
  if (to_int(player) < 0 || to_int(player) >= PLAYER_NO)
    return false;

  auto moves = generate_legal_moves(position, player);
  return std::find(moves.begin(), moves.end(), move) != moves.end();
}

std::optional<BughouseUndo> try_apply_move(BughousePosition &position,
                                           PlayerId player, Move move) {
  if (!is_legal_move(position, player, move))
    return std::nullopt;
  return apply_move(position, player, move);