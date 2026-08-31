#include "agent/replay.h"
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
struct Opportunity {
  size_t event_index = 0;
  bool active = false;
};

struct PendingDrop {
  PlayerId actor = NO_PLAYER;
  Square square = -1;
  bool waste_eligible = false;
  bool coordination_candidate = false;
  size_t coordination_credit = 0;
  bool active = false;
};

constexpr size_t MAX_REPLAY_EVENTS = 100000;
constexpr size_t MAX_REPLAY_HISTORY = 100000;
constexpr int MAX_REPLAY_POCKET_COUNT = 64;

void require(bool condition, const std::string &message) {
  if (!condition)
    throw std::runtime_error("invalid replay: " + message);
}

void require_token(std::istream &in, const char *expected) {
  std::string token;
  if (!(in >> token) || token != expected)
    throw std::runtime_error(std::string("invalid replay: expected ") +
                             expected);
}

void print_board(std::ostream &out, const Board &board, char label) {
  out << "Board " << label << ":\n";
  for (int rank = 7; rank >= 0; rank--) {
    out << rank + 1 << ' ';
    for (int file = 0; file < 8; file++) {
      Piece piece = board.piece_on(to_square(file, rank));
      out << (piece.is_empty() ? '.' : piece.to_char()) << ' ';
    }
    out << '\n';
  }
  out << "  a b c d e f g h\n";
}

void print_pockets(std::ostream &out,
                   const std::array<Pocket, PLAYER_NO> &pockets) {
  out << "Pockets:";
  for (int player = 0; player < PLAYER_NO; player++) {
    out << " P" << player << '[';
    bool first = true;
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      int count = pockets[player].count(static_cast<PieceType>(pt));
      if (!count)
        continue;
      if (!first)
        out << ' ';
      out << Piece{static_cast<PieceType>(pt), WHITE}.to_char() << 'x' << count;
      first = false;
    }
    out << ']';
  }
  out << '\n';
}

void print_current_players(std::ostream &out,
                           const BughousePosition &position) {
  out << "Current players: A=P"
      << to_int(player_on_board(0, position.boards[0].sideToMove)) << " B=P"
      << to_int(player_on_board(1, position.boards[1].sideToMove)) << '\n';
}

void validate_replay_events(const GameReplay &replay) {
  BughousePosition position = replay.initial_state.position;
  for (size_t index = 0; index < replay.events.size(); ++index) {
    const ReplayEvent &event = replay.events[index];
    require(event.event_index == index, "event indices are not contiguous");
    require(event.board == board_of(event.actor),
            "event board does not match actor's board");
    require(
        player_on_board(event.board, position.boards[event.board].sideToMove) ==
            event.actor,
        "event actor is not side to move");
    require(try_apply_move(position, event.actor, event.move).has_value(),
            "illegal event move");
    require(position.pockets == event.pockets_after,
            "event pocket snapshot does not match move result");
    for (int64_t clock : event.clocks_after_ms)
      require(clock >= 0, "negative event clock");
    if (event.message) {
      const Message &message = *event.message;
      require(message.sender == event.actor,
              "message sender does not match event actor");
      require(std::isfinite(message.piece_request.confidence) &&
                  message.piece_request.confidence >= 0.f &&
                  message.piece_request.confidence <= 1.f,
              "invalid piece request confidence");
      require(std::isfinite(message.strat_request.confidence) &&
                  message.strat_request.confidence >= 0.f &&
                  message.strat_request.confidence <= 1.f,
              "invalid strategy request confidence");
      require(message.piece_request.eta_plies >= 0,
              "negative piece request ETA");
    }
  }
}
} // namespace

double ReplayMetrics::synchrony_score() const {
  if (!coordination_opportunities)
    return 0.0;
  return static_cast<double>(synchrony_credit) /
         (COORDINATION_WINDOW_EVENTS * coordination_opportunities);
}

double ReplayMetrics::wasted_drop_rate() const {
  return total_drops ? static_cast<double>(wasted_drops) / total_drops : 0.0;
}

ReplayMetrics analyse_replay(const GameReplay &replay) {
  ReplayMetrics metrics;
  BughousePosition position = replay.initial_state.position;
  std::array<std::array<Opportunity, PIECE_TYPE_NO>, PLAYER_NO> opportunities{};
  std::array<PendingDrop, BOARD_NO> pending_drops{};

  for (const ReplayEvent &event : replay.events) {
    for (auto &by_piece : opportunities)
      for (Opportunity &opportunity : by_piece)
        if (opportunity.active &&
            event.event_index >= opportunity.event_index &&
            event.event_index - opportunity.event_index >
                COORDINATION_WINDOW_EVENTS)
          opportunity.active = false;

    PendingDrop &pending = pending_drops[event.board];
    if (pending.active) {
      const Board &board = position.boards[event.board];
      bool captured = event.actor == next_player(pending.actor) &&
                      board.is_capture(event.move) &&
                      event.move.to == pending.square;
      if (captured && pending.waste_eligible)
        metrics.wasted_drops++;
      if (!captured && pending.coordination_candidate) {
        metrics.coordinated_responses++;
        metrics.synchrony_credit += pending.coordination_credit;
      }
      pending.active = false;
    }

    bool coordination_candidate = false;
    size_t coordination_credit = 0;
    if (event.move.is_drop()) {
      Opportunity &opportunity =
          opportunities[to_int(event.actor)][event.move.drop_pt];
      if (opportunity.active) {
        size_t latency = event.event_index - opportunity.event_index;
        if (latency >= 1 && latency <= COORDINATION_WINDOW_EVENTS) {
          coordination_candidate = true;
          coordination_credit = COORDINATION_WINDOW_EVENTS + 1 - latency;
        }
        opportunity.active = false;
      }
    }

    Board &board = position.boards[event.board];
    bool defensive_drop = event.move.is_drop() && board.is_in_check();
    PlayerId recipient = partner_of(event.actor);
    std::array<int, PIECE_TYPE_NO> recipient_before{};
    for (int pt = PAWN; pt <= QUEEN; pt++)
      recipient_before[pt] =
          position.pockets[to_int(recipient)].count(static_cast<PieceType>(pt));

    std::optional<BughouseUndo> undo_opt =
        try_apply_move(position, event.actor, event.move);
    if (!undo_opt)
      throw std::runtime_error("invalid replay: illegal move at event " +
                               std::to_string(event.event_index));
    const BughouseUndo &undo = *undo_opt;
    if (undo.creditedPartner && recipient_before[undo.creditedPiece] == 0) {
      Opportunity &opportunity =
          opportunities[to_int(recipient)][undo.creditedPiece];
      opportunity = Opportunity{event.event_index, true};
      metrics.coordination_opportunities++;
    }

    if (event.move.is_drop()) {
      metrics.total_drops++;
      bool gives_check = position.boards[event.board].is_in_check();
      bool forcing_response =
          coordination_candidate && (gives_check || defensive_drop);
      if (forcing_response) {
        metrics.coordinated_responses++;
        metrics.synchrony_credit += coordination_credit;
      }
      pending_drops[event.board] =
          PendingDrop{event.actor,
                      event.move.to,
                      !gives_check && !defensive_drop,
                      coordination_candidate && !forcing_response,
                      coordination_credit,
                      true};
    }
  }
  return metrics;
}

void write_replay(std::ostream &out, const GameReplay &replay) {
  out << std::setprecision(std::numeric_limits<float>::max_digits10);
  out << "BUGHOUSE_REPLAY 1\n";
  out << "BOARD0 "
      << std::quoted(replay.initial_state.position.boards[0].to_fen()) << '\n';
  out << "BOARD1 "
      << std::quoted(replay.initial_state.position.boards[1].to_fen()) << '\n';
  out << "POCKETS";
  for (int player = 0; player < PLAYER_NO; player++)
    for (int pt = PAWN; pt <= QUEEN; pt++)
      out << ' '
          << replay.initial_state.position.pockets[player].count(
                 static_cast<PieceType>(pt));
  out << "\nCLOCKS";
  for (int player = 0; player < PLAYER_NO; player++)
    out << ' ' << replay.initial_state.clock.time_ms[player];
  out << ' ' << replay.initial_state.clock.increment_ms << "\nHISTORY "
      << replay.initial_state.history.size() << '\n';
  for (const RepetitionNode &node : replay.initial_state.history)
    out << node.key << ' ' << node.reversible_plies << ' ' << node.repetition
        << '\n';
  out << "EVENTS " << replay.events.size() << '\n';
  for (const ReplayEvent &event : replay.events) {
    out << "EVENT " << event.event_index << ' ' << to_int(event.actor) << ' '
        << event.board << ' ' << event.move.from << ' ' << event.move.to << ' '
        << static_cast<int>(event.move.type) << ' '
        << static_cast<int>(event.move.promote_pt) << ' '
        << static_cast<int>(event.move.drop_pt);
    for (int player = 0; player < PLAYER_NO; player++)
      out << ' ' << event.clocks_after_ms[player];
    for (int player = 0; player < PLAYER_NO; player++)
      for (int pt = PAWN; pt <= QUEEN; pt++)
        out << ' '
            << event.pockets_after[player].count(static_cast<PieceType>(pt));
    out << ' ' << event.message.has_value();
    if (event.message) {
      const Message &m = *event.message;
      out << ' ' << to_int(m.sender) << ' ' << m.move_no << ' '
          << static_cast<int>(m.piece_request.piece) << ' '
          << m.piece_request.confidence << ' '
          << static_cast<int>(m.piece_request.urgency) << ' '
          << m.piece_request.eta_plies << ' '
          << static_cast<int>(m.strat_request.strat) << ' '
          << m.strat_request.confidence << ' '
          << static_cast<int>(m.strat_request.urgency);
    }
    out << '\n';
  }
  out << "RESULT " << static_cast<int>(replay.terminal_result) << '\n';
}

GameReplay read_replay(std::istream &in) {
  GameReplay replay;
  require_token(in, "BUGHOUSE_REPLAY");
  int version = 0;
  if (!(in >> version) || version != 1)
    throw std::runtime_error("unsupported replay version");
  std::string fen;
  require_token(in, "BOARD0");
  in >> std::quoted(fen);
  if (!replay.initial_state.position.boards[0].load_fen(fen))
    throw std::runtime_error("invalid replay board 0");
  require_token(in, "BOARD1");
  in >> std::quoted(fen);
  if (!replay.initial_state.position.boards[1].load_fen(fen))
    throw std::runtime_error("invalid replay board 1");
  require_token(in, "POCKETS");
  replay.initial_state.position.pockets = {};
  for (int player = 0; player < PLAYER_NO; player++)
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      int count = 0;
      in >> count;
      require(count >= 0 && count <= MAX_REPLAY_POCKET_COUNT,
              "invalid pocket count");
      for (int i = 0; i < count; i++)
        replay.initial_state.position.pockets[player].add(
            static_cast<PieceType>(pt));
    }
  require_token(in, "CLOCKS");
  for (int player = 0; player < PLAYER_NO; player++)
    in >> replay.initial_state.clock.time_ms[player];
  in >> replay.initial_state.clock.increment_ms;
  replay.initial_state.clock.active_players.fill(NO_PLAYER);
  require_token(in, "HISTORY");
  size_t history_count = 0;
  in >> history_count;
  require(history_count <= MAX_REPLAY_HISTORY, "too many history nodes");
  replay.initial_state.history.resize(history_count);
  for (RepetitionNode &node : replay.initial_state.history)
    in >> node.key >> node.reversible_plies >> node.repetition;
  require_token(in, "EVENTS");
  size_t event_count = 0;
  in >> event_count;
  require(event_count <= MAX_REPLAY_EVENTS, "too many events");
  replay.events.reserve(event_count);
  for (size_t i = 0; i < event_count; i++) {
    require_token(in, "EVENT");
    ReplayEvent event;
    int actor = 0, move_type = 0, promote = 0, drop = 0, has_message = 0;
    in >> event.event_index >> actor >> event.board >> event.move.from >>
        event.move.to >> move_type >> promote >> drop;
    require(actor >= 0 && actor < PLAYER_NO, "event actor out of range");
    require(event.board == 0 || event.board == 1, "event board out of range");
    require(event.board == board_of(to_player(actor)),
            "event board does not match actor's board");
    require(move_type >= NORMAL && move_type <= DROP, "bad move type");
    require(promote >= NO_PIECE_TYPE && promote <= KING,
            "bad promotion piece type");
    require(drop >= NO_PIECE_TYPE && drop <= KING, "bad drop piece type");
    event.actor = to_player(actor);
    event.move.type = static_cast<MoveType>(move_type);
    event.move.promote_pt = static_cast<PieceType>(promote);
    event.move.drop_pt = static_cast<PieceType>(drop);
    for (int player = 0; player < PLAYER_NO; player++)
      in >> event.clocks_after_ms[player];
    for (int player = 0; player < PLAYER_NO; player++)
      for (int pt = PAWN; pt <= QUEEN; pt++) {
        int count = 0;
        in >> count;
        require(count >= 0 && count <= MAX_REPLAY_POCKET_COUNT,
                "invalid pocket count");
        for (int n = 0; n < count; n++)
          event.pockets_after[player].add(static_cast<PieceType>(pt));
      }
    in >> has_message;
    if (has_message) {
      Message message;
      int sender = 0, piece = 0, piece_urgency = 0, strategy = 0,
          strategy_urgency = 0;
      in >> sender >> message.move_no >> piece >>
          message.piece_request.confidence >> piece_urgency >>
          message.piece_request.eta_plies >> strategy >>
          message.strat_request.confidence >> strategy_urgency;
      require(sender >= 0 && sender < PLAYER_NO, "message sender out of range");
      require(piece >= NO_PIECE_TYPE && piece <= KING,
              "bad requested piece type");
      require(piece_urgency >= 0 &&
                  piece_urgency <= static_cast<int>(Urgency::Critical),
              "bad piece request urgency");
      require(strategy >= 0 && strategy <= static_cast<int>(StrategyType::Flag),
              "bad strategy type");
      require(strategy_urgency >= 0 &&
                  strategy_urgency <= static_cast<int>(Urgency::Critical),
              "bad strategy request urgency");
      message.sender = to_player(sender);
      message.piece_request.piece = static_cast<PieceType>(piece);
      message.piece_request.urgency = static_cast<Urgency>(piece_urgency);
      message.strat_request.strat = static_cast<StrategyType>(strategy);
      message.strat_request.urgency = static_cast<Urgency>(strategy_urgency);
      event.message = message;
    }
    replay.events.push_back(std::move(event));
  }
  require_token(in, "RESULT");
  int result = 0;
  in >> result;
  require(result >= static_cast<int>(GameResult::ONGOING) &&
              result <= static_cast<int>(GameResult::DRAW),
          "bad terminal result");
  replay.terminal_result = static_cast<GameResult>(result);
  if (!in)
    throw std::runtime_error("truncated replay");
  validate_replay_events(replay);
  return replay;
}

void view_replay(std::ostream &out, const GameReplay &replay, bool step_by_step,
                 std::istream *step_input) {
  BughousePosition position = replay.initial_state.position;
  print_board(out, position.boards[0], 'A');
  print_board(out, position.boards[1], 'B');
  print_pockets(out, position.pockets);
  print_current_players(out, position);
  for (const ReplayEvent &event : replay.events) {
    if (!try_apply_move(position, event.actor, event.move))
      throw std::runtime_error("invalid replay: illegal move at event " +
                               std::to_string(event.event_index));
    out << "\nEvent " << event.event_index << ": player " << to_int(event.actor)
        << " board " << (event.board == 0 ? 'A' : 'B') << " move "
        << event.move.to_string() << '\n';
    if (event.message)
      out << "Message: piece="
          << static_cast<int>(event.message->piece_request.piece)
          << " strategy="
          << static_cast<int>(event.message->strat_request.strat) << '\n';
    print_board(out, position.boards[0], 'A');
    print_board(out, position.boards[1], 'B');
    print_pockets(out, event.pockets_after);
    print_current_players(out, position);
    out << "Clocks:";
    for (int64_t clock : event.clocks_after_ms)
      out << ' ' << clock;
    out << '\n';
    if (step_by_step && step_input) {
      out << "Press Enter for next event..." << std::flush;
      std::string line;
      std::getline(*step_input, line);
    }
  }
  out << "Terminal result: " << static_cast<int>(replay.terminal_result)
      << '\n';
}