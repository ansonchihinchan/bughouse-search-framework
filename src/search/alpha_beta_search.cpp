#include "search/alpha_beta_search.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include "search/see.h"
#include <algorithm>

int AlphaBetaSearch::search_first_move(BughousePosition &position,
                                       const SearchContext &next,
                                       const DetailedMove &prev, int depth,
                                       int alpha, int beta, int ply,
                                       std::stop_token stop_token) {
  return -alpha_beta(position, next, prev, depth - 1, -beta, -alpha, ply + 1,
                     stop_token);
}

int AlphaBetaSearch::search_tail_move(BughousePosition &position,
                                      const SearchContext &next,
                                      const DetailedMove &prev, int depth,
                                      int alpha, int beta, int ply,
                                      int reduction,
                                      std::stop_token stop_token) {
  int score;
  if (reduction > 0) {
    score = -alpha_beta(position, next, prev, depth - 1 - reduction, -beta,
                        -alpha, ply + 1, stop_token);
    if (score > alpha)
      score = -alpha_beta(position, next, prev, depth - 1, -beta, -alpha,
                          ply + 1, stop_token);
  } else {
    score = -alpha_beta(position, next, prev, depth - 1, -beta, -alpha, ply + 1,
                        stop_token);
  }
  return score;
}

int AlphaBetaSearch::quiescence(BughousePosition &position,
                                const SearchContext &context, int alpha,
                                int beta, int qply,
                                std::stop_token stop_token) {
  // TODO: check extensions
  stats_.nodes++;

  if (stop_token.stop_requested() || deadline_reached())
    return evaluator_.evaluate(position, context);

  const Board &board = position.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();
  Colour mover_colour = colour_of_player(context.root_player);

  if (in_check && qply >= params_.quiescence_max_ply)
    return evaluator_.evaluate(position, context);

  int stand_pat = 0;

  if (!in_check) {
    stand_pat = evaluator_.evaluate(position, context);

    if (stand_pat >= beta)
      return stand_pat;

    alpha = std::max(alpha, stand_pat);
  }

  std::vector<Move> moves = generate_legal_moves(position, context.root_player);

  if (in_check) {
    if (moves.empty())
      return -INF_SCORE + qply;
  } else {
    // Keep only captures and forcing drops
    std::erase_if(moves, [&](const Move &m) {
      return !((board.is_capture(m)) ||
               (m.is_drop() &&
                drop_gives_check(board, m.drop_pt, m.to, mover_colour)));
    });

    // SEE filtering, Delta pruning
    std::erase_if(moves, [&](const Move &m) {
      if (m.is_drop())
        return false;

      SEE::Result see = SEE::see_result(board, m);

      if (see.score < -50)
        return true;

      return stand_pat + see.score + params_.delta_margin < alpha;
    });
  }

  std::sort(moves.begin(), moves.end(), [&](const Move &a, const Move &b) {
    int score_a =
        a.is_drop() ? SEE::PIECE_VALUE[a.drop_pt] + SEE::POCKET_BONUS[a.drop_pt]
                    : SEE::see_score(board, a);
    int score_b =
        b.is_drop() ? SEE::PIECE_VALUE[b.drop_pt] + SEE::POCKET_BONUS[b.drop_pt]
                    : SEE::see_score(board, b);
    return score_a > score_b;
  });

  for (Move move : moves) {
    BughouseUndo undo = apply_move(position, context.root_player, move);

    int score = -quiescence(
        position, make_context(context.clock, next_player(context.root_player)),
        -beta, -alpha, qply + 1, stop_token);
    undo_move(position, context.root_player, move, undo);

    if (score >= beta)
      return score;

    alpha = std::max(alpha, score);

    if (stop_token.stop_requested() || deadline_reached())
      break;
  }
  return alpha;
}