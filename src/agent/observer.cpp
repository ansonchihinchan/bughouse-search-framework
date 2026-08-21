#include "agent/observer.h"
#include <algorithm>
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string_view>
#include <unistd.h>

namespace {
constexpr int BOARD_PANEL_WIDTH = 18;

std::string_view result_name(GameResult result) {
  switch (result) {
  case GameResult::ONGOING:
    return "ongoing";
  case GameResult::TEAM_A_WINS:
    return "team A wins";
  case GameResult::TEAM_B_WINS:
    return "team B wins";
  case GameResult::DRAW:
    return "draw";
  }
  return "unknown";
}

std::string_view strategy_name(StrategyType strategy) {
  switch (strategy) {
  case StrategyType::None:
    return "none";
  case StrategyType::AvoidTrades:
    return "avoid trades";
  case StrategyType::TradeEverything:
    return "tradeeverything";
  case StrategyType::AttackNow:
    return "attack now";
  case StrategyType::Defend:
    return "defend";
  case StrategyType::Stall:
    return "stall";
  case StrategyType::Flag:
    return "flag";
  }
  return "unknown";
}

std::string_view urgency_name(Urgency urgency) {
  switch (urgency) {
  case Urgency::Low:
    return "low";
  case Urgency::Medium:
    return "medium";
  case Urgency::High:
    return "high";
  case Urgency::Critical:
    return "critical";
  }
  return "unknown";
}

std::string piece_name(PieceType type) {
  if (type < PAWN || type > KING)
    return "none";
  return std::string(1, Piece{type, WHITE}.to_char());
}

std::string clock_text(int64_t milliseconds) {
  milliseconds = std::max<int64_t>(milliseconds, 0);
  int64_t minutes = milliseconds / 60000;
  int64_t seconds = (milliseconds / 1000) % 60;
  int64_t millis = milliseconds % 1000;
  std::ostringstream out;
  out << minutes << ':' << std::setfill('0') << std::setw(2) << seconds << '.'
      << std::setw(3) << millis;
  return out.str();
}

std::array<std::string, 10> board_lines(const Board &board) {
  std::array<std::string, 10> lines;
  for (int row = 0; row < 8; row++) {
    int rank = 7 - row;
    std::ostringstream line;
    line << rank + 1 << ' ';
    for (int file = 0; file < 8; file++) {
      Piece piece = board.piece_on(to_square(file, rank));
      line << (piece.is_empty() ? '.' : piece.to_char());
      if (file != 7)
        line << ' ';
    }
    lines[row] = line.str();
  }
  lines[8] = "  a b c d e f g h";
  lines[9] = "";
  return lines;
}
} // namespace

std::string
TerminalRenderer::render_boards(const BughousePosition &position) const {
  const auto left = board_lines(position.boards[0]);
  const auto right = board_lines(position.boards[1]);
  std::ostringstream out;
  out << std::left << std::setw(BOARD_PANEL_WIDTH) << "[Board A]" << "  "
      << "[Board B]\n";
  for (size_t line = 0; line < left.size() - 1; line++)
    out << std::left << std::setw(BOARD_PANEL_WIDTH) << left[line] << "  "
        << right[line] << '\n';
  return out.str();
}

std::string
TerminalRenderer::render_pockets(const BughousePosition &position) const {
  std::ostringstream out;
  out << "Pockets:";
  for (int player = 0; player < PLAYER_NO; player++) {
    out << " P" << player << '[';
    bool first = true;
    for (int type = PAWN; type <= QUEEN; type++) {
      int count = position.pockets[player].count(static_cast<PieceType>(type));
      if (!count)
        continue;
      if (!first)
        out << ' ';
      out << piece_name(static_cast<PieceType>(type)) << 'x' << count;
      first = false;
    }
    out << ']';
  }
  return out.str();
}

std::string TerminalRenderer::render_clocks(const BughouseClock &clock) const {
  std::ostringstream out;
  out << "Clocks:";
  for (int player = 0; player < PLAYER_NO; player++)
    out << " P" << player << '='
        << clock_text(clock.remaining(to_player(player)));
  return out.str();
}

std::string TerminalRenderer::render_message(const Message &message) const {
  std::ostringstream out;
  out << "Message: P" << to_int(message.sender) << " move=" << message.move_no;
  if (message.piece_request.piece != NO_PIECE_TYPE)
    out << " piece=" << piece_name(message.piece_request.piece)
        << " eta=" << message.piece_request.eta_plies
        << " confidence=" << message.piece_request.confidence
        << " urgency=" << urgency_name(message.piece_request.urgency);
  if (message.strat_request.strat != StrategyType::None)
    out << " strategy=" << strategy_name(message.strat_request.strat)
        << " confidence=" << message.strat_request.confidence
        << " urgency=" << urgency_name(message.strat_request.urgency);
  return out.str();
}

std::string TerminalRenderer::render_event(const ReplayEvent &event) const {
  std::ostringstream out;
  out << "Ply " << event.event_index + 1 << ": P" << to_int(event.actor)
      << " board=" << (event.board == 0 ? 'A' : 'B')
      << " move=" << event.move.to_string();
  if (event.message)
    out << '\n' << render_message(*event.message);
  else
    out << "\nMessage: none";
  return out.str();
}

std::string TerminalRenderer::render_game_state(
    const BughouseState &state,
    const std::optional<ReplayEvent> &last_event) const {
  std::ostringstream out;
  out << "--- Self-play ---\n"
      << render_boards(state.position) << render_pockets(state.position) << '\n'
      << render_clocks(state.clock) << "\nCurrent: A=P"
      << to_int(player_on_board(0, state.position.boards[0].sideToMove))
      << " B=P"
      << to_int(player_on_board(1, state.position.boards[1].sideToMove))
      << '\n';
  if (last_event)
    out << render_event(*last_event) << '\n';
  else
    out << "Ply 0\nMessage: none\n";
  return out.str();
}

std::string
TerminalRenderer::render_game_end(const SelfPlayResult &result) const {
  std::ostringstream out;
  out << "Result: " << result_name(result.game_result)
      << " plies=" << result.plies << '\n'
      << "Coord.: score=" << result.synchrony_score
      << " opportunities=" << result.coordination_opportunities
      << " responses=" << result.coordinated_responses << '\n'
      << "Drops: wasted=" << result.wasted_drops << '/' << result.total_drops
      << " rate=" << result.wasted_drop_rate << '\n'
      << "Requests: messages=" << result.messages_sent
      << " fulfilled=" << result.requests_fulfilled << '\n'
      << "Transfers: " << result.piece_transfers
      << " sacrifices=" << result.agent_metrics.sacrifices_accepted
      << " partner-uses=" << result.accepted_sacrifice_transfers_used << '\n';
  return out.str();
}

std::string TerminalRenderer::render_tournament_start(
    const TournamentConfig &config) const {
  std::ostringstream out;
  out << "--- tournament live ---\nTournament seed=" << config.matchup.seed
      << " games=" << config.game_count << "\nSeats:";
  for (int player = 0; player < PLAYER_NO; player++)
    out << " P" << player << '='
        << agent_type_name(config.matchup.agent_configs[player].type);
  out << '\n';
  return out.str();
}

std::string
TerminalRenderer::render_tournament_progress(const TournamentGameRecord &record,
                                             const TournamentSummary &summary,
                                             size_t configured_games) const {
  std::ostringstream out;
  out << "Game " << summary.games << " / " << configured_games
      << " index=" << record.game_index << " seed=" << record.seed << '\n'
      << "Team A wins=" << summary.team_a_wins
      << " Team B wins=" << summary.team_b_wins << " Draws=" << summary.draws
      << " Unfinished=" << summary.unfinished << '\n'
      << "Synchrony score=" << summary.synchrony_score()
      << " responses=" << summary.coordinated_responses << '/'
      << summary.coordination_opportunities << '\n'
      << "Wasted-drop rate=" << summary.wasted_drop_rate() << " ("
      << summary.wasted_drops << '/' << summary.total_drops << ")\n";
  return out.str();
}

std::string
TerminalRenderer::render_tournament_end(const TournamentResult &result) const {
  std::ostringstream out;
  out << "--- tournament complete ---\n"
      << "Games=" << result.summary.games
      << " Team A wins=" << result.summary.team_a_wins
      << " Team B wins=" << result.summary.team_b_wins
      << " Draws=" << result.summary.draws
      << " Unfinished=" << result.summary.unfinished << '\n'
      << "Synchrony score=" << result.summary.synchrony_score()
      << " Wasted-drop rate=" << result.summary.wasted_drop_rate() << '\n';
  return out.str();
}

TerminalObserver::TerminalObserver(std::ostream &out, bool live)
    : out_(out), live_(live && &out == &std::cout && ::isatty(STDOUT_FILENO)) {}

void TerminalObserver::emit(const std::string &text, bool refresh) {
  if (live_ && refresh)
    out_ << "\x1b[2J\x1b[H";
  out_ << text;
  if (live_)
    out_ << std::flush;
}

void TerminalObserver::on_game_start(const BughouseState &initial_state) {
  last_frame_ = renderer_.render_game_state(initial_state);
  emit(last_frame_, true);
}

void TerminalObserver::on_ply(const ReplayEvent &event,
                              const BughouseState &state) {
  last_frame_ = renderer_.render_game_state(state, event);
  emit(last_frame_, true);
}

void TerminalObserver::on_game_end(const SelfPlayResult &result) {
  emit(renderer_.render_game_end(result), false);
}

void TerminalObserver::on_tournament_start(const TournamentConfig &config) {
  tournament_games_ = config.game_count;
  emit(renderer_.render_tournament_start(config), true);
}

void TerminalObserver::on_game_end(const TournamentGameRecord &record,
                                   const TournamentSummary &summary) {
  emit(renderer_.render_tournament_progress(record, summary, tournament_games_),
       false);
}

void TerminalObserver::on_tournament_end(const TournamentResult &result) {
  emit(renderer_.render_tournament_end(result), false);
}