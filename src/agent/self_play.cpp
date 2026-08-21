#include "agent/self_play.h"
#include "game/movegen.h"
#include <algorithm>
#include <stdexcept>

SelfPlayResult SelfPlayRunner::run(BughouseState &game,
                                   const SelfPlayConfig &config,
                                   std::stop_token stop_token) {
  if (config.first_board < 0 || config.first_board >= BOARD_NO)
    throw std::invalid_argument("self-play first board is out of range");
  if (config.simulated_move_cost_ms < 0)
    throw std::invalid_argument("self-play move cost cannot be negative");

  SelfPlayResult result;
  int preferred_board = config.first_board;
  std::array<std::optional<PieceType>, PLAYER_NO> pending_requests{};
  std::array<std::array<size_t, PIECE_TYPE_NO>, PLAYER_NO>
      pending_sacrifice_transfers{};
  for (int i = 0; i < PLAYER_NO; i++)
    result.strategies[i] = experiment_.agent(to_player(i)).type();

  // Self-play uses configured, deterministic move costs rather than wall time.
  game.clock.active_players.fill(NO_PLAYER);

  auto finish = [&](SelfPlayTermination termination) {
    result.termination = termination;
    result.game_result = game.result();
    for (int i = 0; i < PLAYER_NO; i++)
      result.final_clocks_ms[i] = game.clock.remaining(to_player(i));
    return result;
  };

  while (result.plies < config.max_plies) {
    result.game_result = game.result();
    if (result.game_result != GameResult::ONGOING)
      return finish(SelfPlayTermination::GameOver);
    if (stop_token.stop_requested())
      return finish(SelfPlayTermination::Stopped);

    int selected_board = -1;
    PlayerId selected_player = NO_PLAYER;
    std::vector<Move> legal;
    for (int offset = 0; offset < BOARD_NO; ++offset) {
      int board = (preferred_board + offset) % BOARD_NO;
      PlayerId player =
          player_on_board(board, game.position.boards[board].sideToMove);
      auto candidates = generate_legal_moves(game.position, player);
      if (!candidates.empty()) {
        selected_board = board;
        selected_player = player;
        legal = std::move(candidates);
        break;
      }
    }

    if (selected_board < 0) {
      return finish(SelfPlayTermination::NoLegalMove);
    }

    AgentOutput output = experiment_.choose_move(
        game, selected_player, config.search_limits, stop_token);
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

    if (game.clock.time_ms[to_int(selected_player)] <=
        config.simulated_move_cost_ms) {
      game.clock.time_ms[to_int(selected_player)] = 0;
      return finish(SelfPlayTermination::GameOver);
    }

    BughouseUndo undo = game.make_move(selected_player, move);
    game.clock.time_ms[to_int(selected_player)] -=
        config.simulated_move_cost_ms;
    game.clock.time_ms[to_int(selected_player)] += game.clock.increment_ms;
    game.clock.active_players.fill(NO_PLAYER);
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
    preferred_board = (selected_board + 1) % BOARD_NO;
  }

  return finish(game.result() == GameResult::ONGOING
                    ? SelfPlayTermination::PlyLimit
                    : SelfPlayTermination::GameOver);
}