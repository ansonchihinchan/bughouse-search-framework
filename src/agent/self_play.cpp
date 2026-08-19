#include "agent/self_play.h"
#include "game/movegen.h"
#include <algorithm>
#include <stdexcept>

SelfPlayResult SelfPlayRunner::run(BughouseState &game,
                                   const SelfPlayConfig &config,
                                   std::stop_token stop_token) {
  if (config.first_board < 0 || config.first_board >= BOARD_NO)
    throw std::invalid_argument("self-play first board is out of range");

  SelfPlayResult result;
  int preferred_board = config.first_board;

  if (game.result() == GameResult::ONGOING) {
    for (int board = 0; board < BOARD_NO; ++board) {
      if (game.clock.active_player(board) == NO_PLAYER) {
        game.clock.start(
            player_on_board(board, game.position.boards[board].sideToMove));
      }
    }
  }

  while (result.plies < config.max_plies) {
    result.game_result = game.result();
    if (result.game_result != GameResult::ONGOING) {
      result.termination = SelfPlayTermination::GameOver;
      return result;
    }
    if (stop_token.stop_requested()) {
      result.termination = SelfPlayTermination::Stopped;
      return result;
    }

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
      result.termination = SelfPlayTermination::NoLegalMove;
      return result;
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
      return result;
    }

    game.make_move(selected_player, move);
    ++result.plies;
    ++result.moves_by_player[to_int(selected_player)];
    if (output.outgoing_message)
      ++result.messages_sent;
    preferred_board = (selected_board + 1) % BOARD_NO;
  }

  result.game_result = game.result();
  result.termination = result.game_result == GameResult::ONGOING
                           ? SelfPlayTermination::PlyLimit
                           : SelfPlayTermination::GameOver;
  return result;
}
