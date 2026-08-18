#include "search/alpha_beta_search.h"
#include "game/bughouse.h"
#include "game/movegen.h"
#include "search/see.h"
#include <algorithm>
#include <bit>
#include <cmath>

int AlphaBetaSearch::search_first_move(BughousePosition &position,
                                       const SearchContext &next,
                                       const DetailedMove &prev, int depth,
                                       int alpha, int beta, int ply, bool is_pv,
                                       std::stop_token stop_token) {
  return -alpha_beta(position, next, prev, depth - 1, -beta, -alpha, ply + 1,
                     is_pv, stop_token);
}

int AlphaBetaSearch::search_tail_move(BughousePosition &position,
                                      const SearchContext &next,
                                      const DetailedMove &prev, int depth,
                                      int alpha, int beta, int ply,
                                      int reduction, bool is_pv,
                                      std::stop_token stop_token) {
  int score = -alpha_beta(position, next, prev, depth - 1 - reduction,
                          -alpha - 1, -alpha, ply + 1, is_pv, stop_token);

  if (reduction > 0 && score > alpha)
    score = -alpha_beta(position, next, prev, depth - 1, -alpha - 1, -alpha,
                        ply + 1, is_pv, stop_token);

  if (score > alpha && score < beta)
    score = -alpha_beta(position, next, prev, depth - 1, -beta, -alpha,
                        ply + 1, is_pv, stop_token);
  return score;
}

int AlphaBetaSearch::quiescence(BughousePosition &position,
                                const SearchContext &context, int alpha,
                                int beta, int qply,
                                std::stop_token stop_token) {
  // TODO: check extensions
  stats_.nodes++;

  if (stop_token.stop_requested() || deadline_reached())
    return evaluate_position(position, context);

  const Board &board = position.boards[board_of(context.root_player)];
  bool in_check = board.is_in_check();
  Colour mover_colour = colour_of(context.root_player);

  int max_qply = static_cast<int>(std::lround(
      params_.quiescence_max_ply *
      (1.0f - evaluator_.volatility(position, context.root_player))));
  if (qply >= max_qply)
    return evaluate_position(position, context);

  int stand_pat = 0;

  if (!in_check) {
    stand_pat = evaluate_position(position, context);

    if (stand_pat >= beta)
      return stand_pat;

    alpha = std::max(alpha, stand_pat);
  }

  std::vector<Move> moves;

  if (in_check) {
    moves = generate_legal_moves(position, context.root_player);
    if (moves.empty())
      return -INF_SCORE + qply;
  } else {
    moves = generate_pseudo_legal_moves(board);
    std::erase_if(moves, [&](const Move &m) {
      return !board.is_capture(m) || !board.is_legal(m);
    });

    const Pocket &pocket =
        position.pockets[to_int(context.root_player)];
    DropCheckMasks checking = drop_check_masks(board, mover_colour);
    for (int pt = PAWN; pt <= QUEEN; pt++) {
      PieceType piece_type = static_cast<PieceType>(pt);
      if (!pocket.contains(piece_type))
        continue;
      Bitboard checks = checking.for_piece(piece_type);
      while (checks) {
        Square to = static_cast<Square>(std::countr_zero(checks));
        checks &= checks - 1;
        Move drop = Move::drop(piece_type, to);
        if (board.is_legal(drop))
          moves.push_back(drop);
      }
    }

    // SEE filtering, (Delta pruning currently removed)
    if (params_.see_enabled) {
      std::erase_if(moves, [&](const Move &m) {
        if (m.is_drop())
          return false;

        SEE::Result see = SEE::see_result(board, m);

        // if (see.score < params_.see_prune_threshold)
        //   return true;
        //
        // return stand_pat + see.score + params_.delta_margin < alpha;
        return see.score < params_.see_prune_threshold;
      });
    }
  }

  std::vector<ScoredMove> scored_moves;
  scored_moves.reserve(moves.size());
  for (const Move &m : moves) {
    int see_score =
        m.is_drop() ? SEE::PIECE_VALUE[m.drop_pt] + SEE::POCKET_BONUS[m.drop_pt]
                    : SEE::see_score(board, m);
    scored_moves.push_back(ScoredMove{m, see_score});
  }

  std::sort(scored_moves.begin(), scored_moves.end(),
            [](const ScoredMove &a, const ScoredMove &b) {
              return a.score > b.score;
            });

  for (const ScoredMove &scored_move : scored_moves) {
    Move move = scored_move.move;
    BughouseUndo undo = apply_move(position, context.root_player, move);

    int score = -quiescence(
        position,
        make_context(context.remaining, next_player(context.root_player),
                     context.comm_context, context.comm_hash),
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
