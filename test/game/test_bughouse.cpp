#include <catch2/catch_all.hpp>

#include "game/bughouse.h"
#include "game/movegen.h"

namespace {
constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr Square sq(char file, int rank) {
  return to_square(file - 'a', rank - 1);
}
} // namespace

TEST_CASE("BughouseState initializes both boards to the standard start",
          "[bughouse][init]") {
  BughouseState game;

  REQUIRE(game.position.boards[0].to_fen() == START_FEN);
  REQUIRE(game.position.boards[1].to_fen() == START_FEN);
  REQUIRE(game.position.boards[0].sideToMove == WHITE);
  REQUIRE(game.position.boards[1].sideToMove == WHITE);
}

TEST_CASE("BughouseState initializes all pockets empty", "[bughouse][init]") {
  BughouseState game;
  for (auto &p : game.position.pockets)
    REQUIRE(p.empty());
}

TEST_CASE("BughouseState initializes clocks for all players",
          "[bughouse][init]") {
  BughouseState game;
  for (int i = 0; i < PLAYER_NO; i++)
    REQUIRE(game.clock.remaining(to_player(i)) == 3 * 60 * 1000);
  REQUIRE(game.clock.increment_ms == 2 * 1000);
}

TEST_CASE("board_of/partner_of map players onto boards/partners correctly",
          "[bughouse][init]") {
  BughouseState game;
  REQUIRE(board_of(to_player(0)) == 0);
  REQUIRE(board_of(to_player(1)) == 0);
  REQUIRE(board_of(to_player(2)) == 1);
  REQUIRE(board_of(to_player(3)) == 1);

  REQUIRE(partner_of(to_player(0)) == 2);
  REQUIRE(partner_of(to_player(2)) == 0);
  REQUIRE(partner_of(to_player(1)) == 3);
  REQUIRE(partner_of(to_player(3)) == 1);
}

TEST_CASE("BughouseState reset restores initial state", "[bughouse][init]") {
  BughouseState state;

  state.position.pockets[0].add(PAWN);
  state.position.pockets[2].add(ROOK);

  state.reset();

  for (int p = 0; p < PLAYER_NO; ++p)
    REQUIRE(state.position.pockets[p].empty());

  Board initial;

  REQUIRE(state.position.boards[0].to_fen() == initial.to_fen());
  REQUIRE(state.position.boards[1].to_fen() == initial.to_fen());
}

TEST_CASE("Normal move can be applied", "[bughouse][apply]") {
  BughouseState state;

  auto moves = generate_legal_moves(state.position, to_player(0));

  REQUIRE_FALSE(moves.empty());

  REQUIRE_NOTHROW(apply_move(state.position, to_player(0), moves.front()));
}

TEST_CASE("Capture credits partner pocket", "[bughouse][apply]") {
  BughouseState state;

  state.position.boards[0].load_fen("4k3/8/8/8/8/8/4p3/4KQ2 w - - 0 1");

  auto moves = generate_legal_moves(state.position, to_player(0));

  auto capture = std::find_if(moves.begin(), moves.end(),
                              [](const Move &m) { return m.to == sq('e', 2); });

  REQUIRE(capture != moves.end());

  apply_move(state.position, to_player(0), *capture);

  REQUIRE(state.position.pockets[2].count(PAWN) == 1);
}

TEST_CASE("Drop move removes piece from pocket", "[bughouse][apply]") {
  BughouseState state;

  state.position.pockets[0].add(KNIGHT);

  auto drops =
      generate_drop_moves(state.position.boards[0], state.position.pockets[0]);

  REQUIRE_FALSE(drops.empty());

  auto drop = drops.front();

  REQUIRE(drop.is_drop());

  apply_move(state.position, to_player(0), drop);

  REQUIRE(state.position.pockets[0].count(drop.drop_pt) == 0);
}

TEST_CASE("undo_move restores a capture", "[bughouse][undo]") {
  BughouseState game;

  game.position.boards[0].load_fen("k7/8/3p4/4P3/8/8/8/7K w - - 0 1");

  auto before = game.position;

  Move exd6 = Move::normal(to_square(4, 4), to_square(3, 5));

  auto undo = apply_move(game.position, to_player(0), exd6);

  undo_move(game.position, to_player(0), exd6, undo);

  REQUIRE(game.position.boards[0].to_fen() == before.boards[0].to_fen());

  REQUIRE(game.position.pockets == before.pockets);
}

TEST_CASE("Undo restores board after normal move", "[bughouse][undo]") {
  BughouseState state;

  auto before = state.position.boards[0].to_fen();

  auto moves = generate_legal_moves(state.position, to_player(0));

  REQUIRE_FALSE(moves.empty());

  auto move = moves.front();

  auto undo = apply_move(state.position, to_player(0), move);

  undo_move(state.position, to_player(0), move, undo);

  REQUIRE(state.position.boards[0].to_fen() == before);
}

TEST_CASE("undo restores a drop", "[bughouse][undo]") {
  BughouseState game;

  game.position.boards[0].load_fen("k7/8/8/8/8/8/8/7K w - - 0 1");

  game.position.pockets[0].add(KNIGHT);

  auto before = game.position;

  Move drop = Move::drop(KNIGHT, to_square(4, 3));

  auto undo = apply_move(game.position, to_player(0), drop);

  undo_move(game.position, to_player(0), drop, undo);

  REQUIRE(game.position.boards[0].to_fen() == before.boards[0].to_fen());

  REQUIRE(game.position.pockets == before.pockets);
}

TEST_CASE("apply undo identity", "[bughouse][undo]") {
  BughouseState game;

  auto moves = generate_legal_moves(game.position, to_player(0));

  for (Move m : moves) {
    auto before = game.position;

    auto undo = apply_move(game.position, to_player(0), m);

    undo_move(game.position, to_player(0), m, undo);

    REQUIRE(game.position.boards == before.boards);

    REQUIRE(game.position.pockets == before.pockets);
  }
}

TEST_CASE("undo removes credited partner piece", "[bughouse][undo]") {
  BughouseState game;

  game.position.boards[0].load_fen("k7/8/3p4/4P3/8/8/8/7K w - - 0 1");

  Move exd6 = Move::normal(to_square(4, 4), to_square(3, 5));

  auto undo = apply_move(game.position, to_player(0), exd6);

  REQUIRE(game.position.pockets[2].count(PAWN) == 1);

  undo_move(game.position, to_player(0), exd6, undo);

  REQUIRE(game.position.pockets[2].empty());
}

TEST_CASE("Checkmate detection", "[bughouse][rules]") {
  BughousePosition pos;

  pos.boards[0].load_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

  REQUIRE(is_checkmate(pos, to_player(1)));
}

TEST_CASE("Stalemate detection", "[bughouse][rules]") {
  BughousePosition pos;

  pos.boards[0].load_fen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");

  REQUIRE(is_stalemate(pos, to_player(1)));
}

TEST_CASE("New game is ongoing", "[bughouse][result]") {
  BughouseState state;

  REQUIRE(state.result() == GameResult::ONGOING);
}

TEST_CASE("Checkmate returns winning team", "[bughouse][result]") {
  BughouseState state;

  state.position.boards[0].load_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1");

  REQUIRE(state.result() == GameResult::TEAM_A_WINS);
}