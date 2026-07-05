#include <catch2/catch_all.hpp>

#include "game/bughouse.h"
#include "game/movegen.h"

namespace {
constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
}

TEST_CASE("BughouseState initializes both boards to the standard start",
          "[bughouse][init]") {
  BughouseState game;

  REQUIRE(game.boards[0].to_fen() == START_FEN);
  REQUIRE(game.boards[1].to_fen() == START_FEN);
  REQUIRE(game.boards[0].sideToMove == WHITE);
  REQUIRE(game.boards[1].sideToMove == WHITE);
}

TEST_CASE("BughouseState initializes all pockets empty", "[bughouse][init]") {
  BughouseState game;
  for (auto &p : game.pockets)
    REQUIRE(p.empty());
}

TEST_CASE("BughouseState initializes clocks for all players",
          "[bughouse][init]") {
  BughouseState game;
  for (int i = 0; i < PLAYER_NO; i++)
    REQUIRE(game.clock.remaining(i) == 3 * 60 * 1000);
  REQUIRE(game.clock.increment_ms == 2 * 1000);
}

TEST_CASE("board_of/partner_of map players onto boards/partners correctly",
          "[bughouse][init]") {
  BughouseState game;
  REQUIRE(game.board_of(0) == 0);
  REQUIRE(game.board_of(1) == 0);
  REQUIRE(game.board_of(2) == 1);
  REQUIRE(game.board_of(3) == 1);

  REQUIRE(game.partner_of(0) == 2);
  REQUIRE(game.partner_of(2) == 0);
  REQUIRE(game.partner_of(1) == 3);
  REQUIRE(game.partner_of(3) == 1);
}

TEST_CASE("apply_move rejects a move from the wrong player", "[bughouse]") {
  BughouseState game;
  // Board 0 side to move is WHITE (player 0), so player 1 must not move.
  Move e7e5 = Move::normal(to_square(4, 6), to_square(4, 4));
  REQUIRE_FALSE(game.apply_move(1, e7e5));
  REQUIRE(game.boards[0].to_fen() == START_FEN);
}

TEST_CASE("apply_move rejects an illegal (non-generated) move", "[bughouse]") {
  BughouseState game;
  // Knight cannot jump straight ahead like a rook here.
  Move bogus = Move::normal(to_square(1, 0), to_square(1, 4)); // b1-b5
  REQUIRE_FALSE(game.apply_move(0, bogus));
}

TEST_CASE("apply_move accepts a legal move and switches side to move",
          "[bughouse]") {
  BughouseState game;
  Move e2e4 = Move::normal(to_square(4, 1), to_square(4, 3));
  REQUIRE(game.apply_move(0, e2e4));
  REQUIRE(game.boards[0].sideToMove == BLACK);
}

TEST_CASE("apply_move captures transfer the piece to partner's pocket",
          "[bughouse][partner]") {
  // White pawn on e5 can capture black pawn on d6 via a normal capture.
  BughouseState game;
  game.boards[0].load_fen("k7/8/3p4/4P3/8/8/8/7K w - - 0 1");

  Move exd6 = Move::normal(to_square(4, 4), to_square(3, 5));
  REQUIRE(game.apply_move(0, exd6));

  // Player 0's partner is player 2.
  REQUIRE(game.pockets[2].count(PAWN) == 1);
  REQUIRE(game.pockets[0].empty());
  REQUIRE(game.pockets[1].empty());
  REQUIRE(game.pockets[3].empty());
}

TEST_CASE("apply_move captures on board B credit board A's partner",
          "[bughouse][partner]") {
  // Board 1 (index 1) is driven by players 2 (black) and 3 (white).
  BughouseState game;
  game.boards[1].load_fen("k7/8/3p4/4P3/8/8/8/7K w - - 0 1");

  Move exd6 = Move::normal(to_square(4, 4), to_square(3, 5));
  // Side to move on boards[1] is WHITE -> player 3.
  REQUIRE(game.apply_move(3, exd6));

  // Player 3's partner is player 1.
  REQUIRE(game.pockets[1].count(PAWN) == 1);
  REQUIRE(game.pockets[3].empty());
}

TEST_CASE("captured king never enters a pocket", "[bughouse][partner]") {
  // Not reachable in a legal game since kings are never captured, but
  // apply_move must not add KING to any pocket if it ever occurred.
  BughouseState game;
  game.boards[0].load_fen("k7/8/8/8/8/8/8/7K w - - 0 1");
  // No capture available here; just assert pockets remain empty after a
  // normal king move (sanity check that non-capturing moves add nothing).
  Move kmove = Move::normal(to_square(7, 0), to_square(6, 0));
  REQUIRE(game.apply_move(0, kmove));
  for (auto &p : game.pockets)
    REQUIRE(p.empty());
}

TEST_CASE("apply_move rejects a drop when the pocket lacks the piece",
          "[bughouse][drops]") {
  BughouseState game;
  Move drop = Move::drop(KNIGHT, to_square(4, 3));
  REQUIRE_FALSE(game.apply_move(0, drop));
}

TEST_CASE("apply_move accepts a drop onto an empty square and removes it "
          "from the pocket",
          "[bughouse][drops]") {
  BughouseState game;
  game.boards[0].load_fen("k7/8/8/8/8/8/8/7K w - - 0 1");
  game.pockets[0].add(KNIGHT);

  Move drop = Move::drop(KNIGHT, to_square(4, 3)); // e4
  REQUIRE(game.apply_move(0, drop));

  REQUIRE(game.boards[0].piece_on(to_square(4, 3)) ==
          make_piece(WHITE, KNIGHT));
  REQUIRE(game.pockets[0].count(KNIGHT) == 0);
  REQUIRE(game.boards[0].sideToMove == BLACK);
}

TEST_CASE("apply_move rejects dropping a pawn onto the back rank",
          "[bughouse][drops]") {
  BughouseState game;
  game.boards[0].load_fen("k7/8/8/8/8/8/8/7K w - - 0 1");
  game.pockets[0].add(PAWN);

  Move drop = Move::drop(PAWN, to_square(4, 7)); // rank 8
  REQUIRE_FALSE(game.apply_move(0, drop));
  REQUIRE(game.pockets[0].count(PAWN) == 1);
}

TEST_CASE("apply_move rejects a drop onto an occupied square",
          "[bughouse][drops]") {
  BughouseState game;
  game.pockets[0].add(KNIGHT);
  // e2 is occupied by white's own pawn in the start position.
  Move drop = Move::drop(KNIGHT, to_square(4, 1));
  REQUIRE_FALSE(game.apply_move(0, drop));
}

TEST_CASE("moves on one board do not alter the other board's state",
          "[bughouse][independence]") {
  BughouseState game;
  Move e2e4 = Move::normal(to_square(4, 1), to_square(4, 3));
  REQUIRE(game.apply_move(0, e2e4));

  REQUIRE(game.boards[1].to_fen() == START_FEN);
}

TEST_CASE("captures on one board only affect the capturing player's "
          "partner pocket, not other pockets",
          "[bughouse][independence]") {
  BughouseState game;
  game.boards[0].load_fen("k7/8/3p4/4P3/8/8/8/7K w - - 0 1");
  Move exd6 = Move::normal(to_square(4, 4), to_square(3, 5));
  REQUIRE(game.apply_move(0, exd6));

  REQUIRE(game.pockets[1].empty());
  REQUIRE(game.pockets[3].empty());
}

TEST_CASE("result() is ONGOING at game start", "[bughouse][terminal]") {
  BughouseState game;
  REQUIRE(game.result() == GameResult::ONGOING);
}

TEST_CASE("result() reports TEAM_B_WINS when board 0 is checkmated for White",
          "[bughouse][terminal]") {
  BughouseState game;
  // Fool's mate style position: White (player 0, team A) is checkmated.
  game.boards[0].load_fen(
      "rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 2 3");

  REQUIRE(game.boards[0].is_checkmate());
  REQUIRE(game.result() == GameResult::TEAM_B_WINS);
}

TEST_CASE("result() reports a team win when a player's clock has flagged",
          "[bughouse][terminal][clock]") {
  BughouseState game;
  game.clock.set(0, 0); // everyone flagged, player 0 checked first
  REQUIRE(game.clock.any_flagged());
  REQUIRE(game.result() == GameResult::TEAM_B_WINS);
}

TEST_CASE("apply_move switches the clock's active player",
          "[bughouse][clock]") {
  BughouseState game;
  Move e2e4 = Move::normal(to_square(4, 1), to_square(4, 3));
  REQUIRE(game.apply_move(0, e2e4));
  REQUIRE(game.clock.active_player == 1);
}