#include "agent/self_play.h"
#include "agent/observer.h"
#include "game/movegen.h"
#include <algorithm>
#include <chrono>
#include <stdexcept>

SearchLimits real_time_search_limits(const SearchLimits &configured,
                                     int64_t remaining_ms, int increment_ms) {
  SearchLimits limits = configured;
  if (limits.infinite)
    return limits;

  int64_t safety_ms =
      std::min<int64_t>(50, std::max<int64_t>(1, remaining_ms / 20));
  int64_t available_ms = std::max<int64_t>(1, remaining_ms - safety_ms);
  int64_t managed_ms =
      std::max<int64_t>(1, remaining_ms / 30 + std::max(0, increment_ms) / 2);
  managed_ms = std::min(managed_ms, available_ms);
  if (limits.move_time.count() <= 0 || limits.move_time.count() > managed_ms)
    limits.move_time = std::chrono::milliseconds(managed_ms);
  return limits;
}

BoardEventScheduler::BoardEventScheduler(int tie_break_board)
    : tie_break_board_(tie_break_board) {
  if (tie_break_board < 0 || tie_break_board >= BOARD_NO)
    throw std::invalid_argument("scheduler tie-break board is out of range");
}

std::optional<ScheduledBoardEvent>
BoardEventScheduler::next_event(const BughousePosition &position) const {
  std::optional<ScheduledBoardEvent> best;
  for (int board = 0; board < BOARD_NO; board++) {
    PlayerId player = player_on_board(board, position.boards[board].sideToMove);
    std::vector<Move> legal = generate_legal_moves(position, player);
    if (legal.empty())
      continue;
    bool earlier = !best || ready_at_ms_[board] < ready_at_ms_[best->board];
    bool tied_and_preferred =
        best && ready_at_ms_[board] == ready_at_ms_[best->board] &&
        board == tie_break_board_ && best->board != tie_break_board_;
    if (earlier || tied_and_preferred)
      best = ScheduledBoardEvent{board, player, std::move(legal)};
  }
  return best;
}

void BoardEventScheduler::complete_event(int board, int64_t elapsed_ms) {
  if (board < 0 || board >= BOARD_NO || elapsed_ms < 0)
    throw std::invalid_argument("invalid completed board event");
  // Clock accounting may round a sub-millisecond decision to zero. The event
  // timeline still advances one tick so the stable tie break cannot starve a
  // second playable board.
  ready_at_ms_[board] += std::max<int64_t>(elapsed_ms, 1);
}

int64_t BoardEventScheduler::ready_at_ms(int board) const {
  if (board < 0 || board >= BOARD_NO)
    throw std::invalid_argument("scheduler board is out of range");
  return ready_at_ms_[board];
}

SelfPlayResult SelfPlayRunner::run(BughouseState &game,
                                   const SelfPlayConfig &config,
                                   std::stop_token stop_token) {
  if (config.first_board < 0 || config.first_board >= BOARD_NO)
    throw std::invalid_argument("self-play first board is out of range");

  if (config.deterministic_move_time_ms < 0)
    throw std::invalid_argument("deterministic move time cannot be negative");
  for (int64_t player_time : config.deterministic_player_move_time_ms)
    if (player_time < 0)
      throw std::invalid_argument(
          "deterministic player time cannot be negative");

  static const SteadyDecisionTimer steady_timer;
  const DecisionTimer &timer =
      config.decision_timer ? *config.decision_timer : steady_timer;

  SelfPlayResult result;
  result.replay.initial_state = game;
  if (config.observer)
    config.observer->on_game_start(game);
  BoardEventScheduler scheduler(config.first_board);
  std::array<std::optional<PieceType>, PLAYER_NO> pending_requests{};
  std::array<std::array<size_t, PIECE_TYPE_NO>, PLAYER_NO>
      pending_sacrifice_transfers{};
  for (int i = 0; i < PLAYER_NO; i++)
    result.strategies[i] = experiment_.agent(to_player(i)).type();

  game.clock.active_players.fill(NO_PLAYER);

  auto finish = [&](SelfPlayTermination termination) {
    result.termination = termination;
    result.game_result = game.result();
    for (int i = 0; i < PLAYER_NO; i++)
      result.final_clocks_ms[i] = game.clock.remaining(to_player(i));
    result.replay.terminal_result = result.game_result;
    ReplayMetrics replay_metrics = analyse_replay(result.replay);
    result.coordination_opportunities =
        replay_metrics.coordination_opportunities;
    result.coordinated_responses = replay_metrics.coordinated_responses;
    result.synchrony_credit = replay_metrics.synchrony_credit;
    result.synchrony_score = replay_metrics.synchrony_score();
    result.total_drops = replay_metrics.total_drops;
    result.wasted_drops = replay_metrics.wasted_drops;
    result.wasted_drop_rate = replay_metrics.wasted_drop_rate();
    if (config.observer)
      config.observer->on_game_end(result);
    return result;
  };

  while (result.plies < config.max_plies) {
    result.game_result = game.result();
    if (result.game_result != GameResult::ONGOING)
      return finish(SelfPlayTermination::GameOver);
    if (stop_token.stop_requested())
      return finish(SelfPlayTermination::Stopped);

    std::optional<ScheduledBoardEvent> scheduled =
        scheduler.next_event(game.position);
    if (!scheduled) {
      return finish(SelfPlayTermination::NoLegalMove);
    }
    int selected_board = scheduled->board;
    PlayerId selected_player = scheduled->player;
    std::vector<Move> &legal = scheduled->legal_moves;

    const int actor_index = to_int(selected_player);
    const int64_t remaining_before = game.clock.time_ms[actor_index];
    SearchLimits decision_limits = config.search_limits;
    DecisionTimer::time_point decision_start{};
    if (config.clock_mode == GameClockMode::RealTime) {
      decision_limits = real_time_search_limits(
          config.search_limits, remaining_before, game.clock.increment_ms);
      decision_start = timer.now();
    }

    AgentOutput output = experiment_.choose_move(game, selected_player,
                                                 decision_limits, stop_token);
    int64_t elapsed_ms = config.deterministic_move_time_ms;
    int64_t player_time = config.deterministic_player_move_time_ms[actor_index];
    if (config.clock_mode == GameClockMode::Deterministic && player_time > 0)
      elapsed_ms = player_time;
    if (config.clock_mode == GameClockMode::RealTime) {
      elapsed_ms = std::max<int64_t>(
          0, std::chrono::duration_cast<std::chrono::milliseconds>(
                 timer.now() - decision_start)
                 .count());
    }

    game.clock.time_ms[actor_index] =
        std::max<int64_t>(0, remaining_before - elapsed_ms);
    if (elapsed_ms >= remaining_before)
      return finish(SelfPlayTermination::GameOver);

    const Move move = output.search_result.best_move;
    if (move.is_none() ||
        std::find(legal.begin(), legal.end(), move) == legal.end()) {
      result.termination = stop_token.stop_requested()
                               ? SelfPlayTermination::Stopped
                               : SelfPlayTermination::InvalidAgentMove;
      if (result.termination == SelfPlayTermination::InvalidAgentMove)
        result.invalid_player = selected_player;
      return finish(result.termination);
    }

    // BughouseState::make_move owns its active-clock transition. Self-play has
    // already charged either measured or deterministic thinking time, so keep
    // that authoritative snapshot across the rules transition.
    const auto clocks_after_thinking = game.clock.time_ms;
    const int increment_ms = game.clock.increment_ms;
    BughouseUndo undo = game.make_move(selected_player, move);
    game.clock.active_players.fill(NO_PLAYER);
    game.clock.time_ms = clocks_after_thinking;
    game.clock.time_ms[actor_index] += increment_ms;
    scheduler.complete_event(selected_board, elapsed_ms);
    result.plies++;
    result.moves_by_player[to_int(selected_player)]++;

    if (move.is_drop() &&
        pending_sacrifice_transfers[to_int(selected_player)][move.drop_pt] >
            0) {
      pending_sacrifice_transfers[to_int(selected_player)][move.drop_pt]--;
      result.accepted_sacrifice_transfers_used++;
      result.successful_temporal_sacrifices++;
    }

    if (undo.creditedPartner) {
      result.piece_transfers++;
      if (output.metrics.sacrifices_accepted > 0) {
        PlayerId recipient = partner_of(selected_player);
        pending_sacrifice_transfers[to_int(recipient)][undo.creditedPiece]++;
        result.accepted_sacrifice_transfers++;
      }
      for (int requester = 0; requester < PLAYER_NO; requester++) {
        if (partner_of(to_player(requester)) == selected_player &&
            pending_requests[requester] == undo.creditedPiece) {
          result.requests_fulfilled++;
          pending_requests[requester].reset();
        }
      }
    }

    result.agent_metrics.sacrifice_attempts +=
        output.metrics.sacrifice_attempts;
    result.agent_metrics.sacrifices_accepted +=
        output.metrics.sacrifices_accepted;
    result.agent_metrics.temporal_transfers_observed +=
        output.metrics.temporal_transfers_observed;
    result.agent_metrics.temporal_partner_uses +=
        output.metrics.temporal_partner_uses;
    if (output.outgoing_message) {
      result.messages_sent++;
      const Message &message = *output.outgoing_message;
      if (message.piece_request.piece != NO_PIECE_TYPE) {
        result.piece_requests_generated++;
        pending_requests[to_int(message.sender)] = message.piece_request.piece;
      }
      if (message.strat_request.strat != StrategyType::None) {
        result.strategy_requests_generated++;
        result.messages_by_strategy[static_cast<size_t>(
            message.strat_request.strat)]++;
      }
    }

    ReplayEvent replay_event;
    replay_event.event_index = result.plies - 1;
    replay_event.actor = selected_player;
    replay_event.board = selected_board;
    replay_event.move = move;
    replay_event.pockets_after = game.position.pockets;

    for (int player = 0; player < PLAYER_NO; player++)
      replay_event.clocks_after_ms[player] =
          game.clock.remaining(to_player(player));

    replay_event.message = output.outgoing_message;
    if (config.observer)
      config.observer->on_ply(replay_event, game);
    result.replay.events.push_back(std::move(replay_event));
  }

  return finish(game.result() == GameResult::ONGOING
                    ? SelfPlayTermination::PlyLimit
                    : SelfPlayTermination::GameOver);
}