#include "search/pvs.h"

int PVS::search_tail_move(BughousePosition &position, const SearchContext &next,
                          const DetailedMove &prev, int depth, int alpha,
                          int beta, int ply, int reduction, bool is_pv,
                          std::stop_token stop_token) {
  int score = -alpha_beta(position, next, prev, depth - 1 - reduction,
                          -alpha - 1, -alpha, ply + 1, is_pv, stop_token);

  if (reduction > 0 && score > alpha)
    score = -alpha_beta(position, next, prev, depth - 1, -alpha - 1, -alpha,
                        ply + 1, is_pv, stop_token);

  if (score > alpha && score < beta)
    score = -alpha_beta(position, next, prev, depth - 1, -beta, -alpha, ply + 1,
                        is_pv, stop_token);

  return score;
}