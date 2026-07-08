#include "search/null_move_search.h"
#include "game/bughouse.h"

int NullMoveSearch::alpha_beta(BughousePosition &position,
                               const SearchContext &context, int depth,
                               int alpha, int beta,
                               std::stop_token stop_token) {
  const Board &board = position.boards[board_of(context.root_player)];
  Colour side = colour_of_player(context.root_player);

  if (depth >= NULL_MOVE_MIN_DEPTH && !board.is_in_check() &&
      board.has_non_pawn(side) && beta < INF_SCORE) {
    BoardUndo undo = make_null_move(position, context.root_player);
    int score = -alpha_beta(
        position, make_context(context.clock, next_player(context.root_player)),
        depth - 1 - NULL_MOVE_REDUCTION, -beta, -beta + 1, stop_token);
    undo_null_move(position, context.root_player, undo);

    if (!stop_token.stop_requested() && !deadline_reached() && score >= beta) {
      stats_.null_move_cutoffs++;
      return score;
    }
  }

  return PVS::alpha_beta(position, context, depth, alpha, beta, stop_token);
}