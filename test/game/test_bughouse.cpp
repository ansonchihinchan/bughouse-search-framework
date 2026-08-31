#include <catch2/catch_all.hpp>

#include "game/bughouse.h"
#include "game/movegen.h"

#include <random>
#include <tuple>

namespace {
constexpr const char *START_FEN =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr Square sq(char file, int rank) {
  return to_square(file - 'a', rank - 1);
}
} // namespace

TEST_CASE("BughouseState initialises both boards to the standard start",
          "[bughouse][init]") {
  BughouseState game;

  REQUIRE(game.position.boards[0].to_fen() == START_FEN);
  REQUIRE(game.position.boards[1].to_fen() == START_FEN);
  REQUIRE(game.position.boards[0].sideToMove == WHITE);
  REQUIRE(game.position.boards[1].sideToMove == WHITE);
}

TEST_CASE("BughouseState initialises all pockets empty", "[bughouse][init]") {
  BughouseState game;
  for (auto &p : game.position.pockets)
    REQUIRE(p.empty());
}

TEST_CASE("BughouseState initialises clocks for all players",
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

TEST_CASE("a legal pocket drop prevents a board-only checkmate result",
          "[bughouse][result][drop]") {
  BughouseState state;
  state.position.boards[0].load_fen("k3r3/8/8/8/8/8/3r1r2/4K3 w - - 0 1");
  state.position.pockets[0].add(ROOK);

  REQUIRE(state.position.boards[0].is_checkmate());
  REQUIRE_FALSE(is_checkmate(state.position, to_player(0)));
  REQUIRE(state.result() == GameResult::ONGOING);
}

TEST_CASE("the 50-move rule is an authoritative game draw",
          "[bughouse][result][draw]") {
  BughouseState state;
  state.position.boards[0].halfMove = HALFMOVE_LIMIT;
  REQUIRE(state.result() == GameResult::DRAW);
}

TEST_CASE("a recorded third repetition is an authoritative game draw",
          "[bughouse][result][draw][repetition]") {
  BughouseState state;
  state.history.push_back(RepetitionNode{position_hash(state.position), 8, -4});
  REQUIRE(state.result() == GameResult::DRAW);
}

TEST_CASE("checkmate takes precedence over the draw counters",
          "[bughouse][result][draw][checkmate]") {
  BughouseState state;
  state.position.boards[0].load_fen("7k/6Q1/6K1/8/8/8/8/8 b - - 100 1");
  state.history.push_back(RepetitionNode{position_hash(state.position), 8, -4});

  REQUIRE(state.result() == GameResult::TEAM_A_WINS);
}

TEST_CASE("pocket hashes distinguish counts above the former clamp",
          "[bughouse][hash][pocket]") {
  BughousePosition fifteen;
  BughousePosition sixteen;
  for (int i = 0; i < 15; ++i) {
    fifteen.pockets[0].add(PAWN);
    sixteen.pockets[0].add(PAWN);
  }
  sixteen.pockets[0].add(PAWN);

  REQUIRE(position_hash(fifteen) != position_hash(sixteen));
}

TEST_CASE("deterministic legal Bughouse sequences undo to exact state and hash",
          "[bughouse][undo][hash][property]") {
  BughousePosition position;
  for (PlayerId player :
       {to_player(0), to_player(1), to_player(2), to_player(3)}) {
    position.pockets[to_int(player)].add(PAWN);
    position.pockets[to_int(player)].add(KNIGHT);
  }
  const BughousePosition initial = position;
  const uint64_t initial_hash = position_hash(position);

  struct AppliedMove {
    PlayerId player;
    Move move;
    BughouseUndo undo;
  };
  std::vector<AppliedMove> applied;
  std::mt19937 generator(0xB09A05E);

  for (int ply = 0; ply < 160; ++ply) {
    const int board_index = ply % BOARD_NO;
    const PlayerId player =
        player_on_board(board_index, position.boards[board_index].sideToMove);
    const std::vector<Move> moves = generate_legal_moves(position, player);
    if (moves.empty())
      break;
    std::uniform_int_distribution<size_t> choose(0, moves.size() - 1);
    const Move move = moves[choose(generator)];
    applied.push_back(
        AppliedMove{player, move, apply_move(position, player, move)});
  }

  REQUIRE(applied.size() >= 40);
  while (!applied.empty()) {
    const AppliedMove entry = applied.back();
    applied.pop_back();
    undo_move(position, entry.player, entry.move, entry.undo);
  }

  INFO("board 0: " << position.boards[0].to_fen() << " expected "
                   << initial.boards[0].to_fen());
  INFO("board 1: " << position.boards[1].to_fen() << " expected "
                   << initial.boards[1].to_fen());
  INFO("board hashes: " << position.boards[0].hash << ", "
                        << position.boards[1].hash << " expected "
                        << initial.boards[0].hash << ", "
                        << initial.boards[1].hash);
  REQUIRE(position.boards == initial.boards);
  REQUIRE(position.pockets == initial.pockets);
  REQUIRE(position_hash(position) == initial_hash);
}

TEST_CASE("make_move stops the mover's clock and starts their board "
          "opponent's",
          "[bughouse][clock]") {
  BughouseState game;
  game.clock.set(10000, 0);
  game.clock.start(to_player(0));

  auto moves = generate_legal_moves(game.position, to_player(0));
  REQUIRE_FALSE(moves.empty());

  game.make_move(to_player(0), moves.front());

  REQUIRE(game.clock.active_player(0) == 1);
}

TEST_CASE("make_move on one board does not disturb the other board's clock",
          "[bughouse][clock]") {
  BughouseState game;
  game.clock.set(10000, 0);
  game.clock.start(to_player(0));
  game.clock.start(to_player(3));

  auto moves = generate_legal_moves(game.position, to_player(0));
  REQUIRE_FALSE(moves.empty());

  game.make_move(to_player(0), moves.front());

  REQUIRE(game.clock.active_player(1) == 3);
}

TEST_CASE("is_legal_move rejects fabricated moves that Board::is_legal alone "
          "would accept",
          "[bughouse][legality]") {
  BughouseState game;

  Move bogus = Move::normal(to_square(4, 3), to_square(4, 4)); // e4-e5, empty
  REQUIRE(game.position.boards[0].is_empty(to_square(4, 3)));
  REQUIRE_FALSE(game.position.boards[0].is_legal(bogus));
  REQUIRE_FALSE(is_legal_move(game.position, to_player(0), bogus));
}

TEST_CASE("is_legal_move rejects a drop of a piece not held in the pocket",
          "[bughouse][legality]") {
  BughouseState game;
  game.position.boards[0].load_fen("k7/8/8/8/8/8/8/7K w - - 0 1");

  Move drop = Move::drop(QUEEN, to_square(4, 3));
  REQUIRE_FALSE(is_legal_move(game.position, to_player(0), drop));

  game.position.pockets[0].add(QUEEN);
  REQUIRE(is_legal_move(game.position, to_player(0), drop));
}

TEST_CASE("try_apply_move fails safely instead of relying on an assert",
          "[bughouse][legality]") {
  BughouseState game;
  Move bogus = Move::normal(to_square(4, 3), to_square(4, 4));

  auto before = game.position;
  auto undo = try_apply_move(game.position, to_player(0), bogus);

  REQUIRE_FALSE(undo.has_value());
  REQUIRE(game.position.boards == before.boards);
  REQUIRE(game.position.pockets == before.pockets);
}