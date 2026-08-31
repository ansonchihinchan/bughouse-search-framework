#include <catch2/catch_all.hpp>

#include "agent/replay.h"
#include "agent/tournament.h"
#include <sstream>

namespace {
void append_event(GameReplay &replay, BughousePosition &position,
                  PlayerId actor, Move move,
                  std::optional<Message> message = std::nullopt) {
  apply_move(position, actor, move);
  ReplayEvent event;
  event.event_index = replay.events.size();
  event.actor = actor;
  event.board = board_of(actor);
  event.move = move;
  event.pockets_after = position.pockets;
  event.clocks_after_ms.fill(60000 - static_cast<int64_t>(event.event_index));
  event.message = message;
  replay.events.push_back(event);
}

GameReplay coordinated_replay() {
  GameReplay replay;
  replay.initial_state.position.boards[0].load_fen(
      "4k3/8/8/8/8/8/p7/R3K3 w - - 0 1");
  replay.initial_state.position.boards[1].load_fen(
      "7k/8/8/8/8/8/8/4K2R b - - 0 1");
  BughousePosition position = replay.initial_state.position;
  append_event(replay, position, to_player(0),
               Move::normal(to_square(0, 0), to_square(0, 1)));
  append_event(replay, position, to_player(2),
               Move::drop(PAWN, to_square(7, 6)));
  replay.terminal_result = GameResult::ONGOING;
  return replay;
}

GameReplay wasted_drop_replay() {
  GameReplay replay;
  replay.initial_state.position.boards[0].load_fen(
      "4k3/8/8/8/8/r7/8/4K3 w - - 0 1");
  replay.initial_state.position.pockets[0].add(KNIGHT);
  BughousePosition position = replay.initial_state.position;
  append_event(replay, position, to_player(0),
               Move::drop(KNIGHT, to_square(0, 1)));
  append_event(replay, position, to_player(1),
               Move::normal(to_square(0, 2), to_square(0, 1)));
  return replay;
}

GameReplay mixed_drop_replay() {
  GameReplay replay;
  replay.initial_state.position.boards[0].load_fen(
      "4k3/8/8/8/8/R7/1p6/1R2K3 w - - 0 1");
  replay.initial_state.position.boards[1].load_fen(
      "7k/8/8/8/8/8/8/4K2R b - - 0 1");
  replay.initial_state.position.pockets[1].add(KNIGHT);
  BughousePosition position = replay.initial_state.position;
  append_event(replay, position, to_player(0),
               Move::normal(to_square(1, 0), to_square(1, 1)));
  append_event(replay, position, to_player(2),
               Move::drop(PAWN, to_square(7, 6)));
  append_event(replay, position, to_player(1),
               Move::drop(KNIGHT, to_square(0, 1)));
  append_event(replay, position, to_player(0),
               Move::normal(to_square(0, 2), to_square(0, 1)));
  return replay;
}
} // namespace

TEST_CASE("synchrony rewards a prompt causal partner response",
          "[agent][replay][metrics][synchrony]") {
  ReplayMetrics coordinated = analyse_replay(coordinated_replay());
  REQUIRE(coordinated.coordination_opportunities == 1);
  REQUIRE(coordinated.coordinated_responses == 1);
  REQUIRE(coordinated.synchrony_credit == COORDINATION_WINDOW_EVENTS);
  REQUIRE(coordinated.synchrony_score() == 1.0);

  GameReplay uncoordinated = coordinated_replay();
  uncoordinated.events.pop_back();
  ReplayMetrics absent = analyse_replay(uncoordinated);
  REQUIRE(absent.coordination_opportunities == 1);
  REQUIRE(absent.coordinated_responses == 0);
  REQUIRE(absent.synchrony_score() == 0.0);
  REQUIRE(analyse_replay(coordinated_replay()).synchrony_score() ==
          coordinated.synchrony_score());
}

TEST_CASE("wasted-drop rate uses auditable raw counts",
          "[agent][replay][metrics][drops]") {
  ReplayMetrics wasted = analyse_replay(wasted_drop_replay());
  REQUIRE(wasted.total_drops == 1);
  REQUIRE(wasted.wasted_drops == 1);
  REQUIRE(wasted.wasted_drop_rate() == 1.0);

  ReplayMetrics useful = analyse_replay(coordinated_replay());
  REQUIRE(useful.total_drops == 1);
  REQUIRE(useful.wasted_drops == 0);
  REQUIRE(useful.wasted_drop_rate() == 0.0);

  ReplayMetrics zero = analyse_replay(GameReplay{});
  REQUIRE(zero.total_drops == 0);
  REQUIRE(zero.wasted_drop_rate() == 0.0);

  ReplayMetrics mixed = analyse_replay(mixed_drop_replay());
  REQUIRE(mixed.total_drops == 2);
  REQUIRE(mixed.wasted_drops == 1);
  REQUIRE(mixed.wasted_drop_rate() == 0.5);

  TournamentSummary aggregate;
  aggregate.coordination_opportunities = 2;
  aggregate.synchrony_credit = COORDINATION_WINDOW_EVENTS;
  aggregate.total_drops = 4;
  aggregate.wasted_drops = 1;
  REQUIRE(aggregate.synchrony_score() == 0.5);
  REQUIRE(aggregate.wasted_drop_rate() == 0.25);
}

TEST_CASE("replay serialization round-trips deterministic event data",
          "[agent][replay][serialization]") {
  GameReplay replay = coordinated_replay();
  Message message;
  message.sender = to_player(2);
  message.move_no = 7;
  message.piece_request.piece = KNIGHT;
  replay.events[1].message = message;
  std::ostringstream encoded;
  write_replay(encoded, replay);
  std::istringstream input(encoded.str());
  GameReplay loaded = read_replay(input);

  REQUIRE(loaded.initial_state.position.boards ==
          replay.initial_state.position.boards);
  REQUIRE(loaded.initial_state.position.pockets ==
          replay.initial_state.position.pockets);
  REQUIRE(loaded.events.size() == 2);
  REQUIRE(loaded.events[0].actor == to_player(0));
  REQUIRE(loaded.events[0].move == replay.events[0].move);
  REQUIRE(loaded.events[1].message.has_value());
  REQUIRE(loaded.events[1].message->piece_request.piece == KNIGHT);
  REQUIRE(loaded.terminal_result == replay.terminal_result);

  std::ostringstream viewed;
  view_replay(viewed, loaded);
  REQUIRE(viewed.str().find("Board A") != std::string::npos);
  REQUIRE(viewed.str().find("Event 1") != std::string::npos);
  REQUIRE(viewed.str().find("Clocks:") != std::string::npos);
  REQUIRE(viewed.str().find("Current players:") != std::string::npos);
}

TEST_CASE("read_replay rejects an out-of-range actor index",
          "[agent][replay][validation]") {
  GameReplay replay = coordinated_replay();
  replay.events[0].actor = to_player(9);

  std::ostringstream encoded;
  write_replay(encoded, replay);
  std::istringstream input(encoded.str());

  REQUIRE_THROWS_AS(read_replay(input), std::runtime_error);
}

TEST_CASE("read_replay rejects an event whose board does not match the "
          "actor's board",
          "[agent][replay][validation]") {
  GameReplay replay = coordinated_replay();
  replay.events[0].board = 1 - replay.events[0].board;

  std::ostringstream encoded;
  write_replay(encoded, replay);
  std::istringstream input(encoded.str());

  REQUIRE_THROWS_AS(read_replay(input), std::runtime_error);
}

TEST_CASE("read_replay rejects an oversized event count",
          "[agent][replay][validation]") {
  std::istringstream input("BUGHOUSE_REPLAY 1\n"
                           "BOARD0 \"4k3/8/8/8/8/8/8/4K3 w - - 0 1\"\n"
                           "BOARD1 \"4k3/8/8/8/8/8/8/4K3 w - - 0 1\"\n"
                           "POCKETS 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n"
                           "CLOCKS 0 0 0 0 0\n"
                           "HISTORY 0\n"
                           "EVENTS 999999999\n");

  REQUIRE_THROWS_AS(read_replay(input), std::runtime_error);
}

TEST_CASE("analyse_replay and view_replay reject an illegal move instead of "
          "invoking undefined behaviour",
          "[agent][replay][validation]") {
  GameReplay replay = coordinated_replay();
  replay.events[0].move = Move::normal(to_square(1, 1), to_square(1, 3));

  REQUIRE_THROWS_AS(analyse_replay(replay), std::runtime_error);

  std::ostringstream out;
  REQUIRE_THROWS_AS(view_replay(out, replay), std::runtime_error);
}