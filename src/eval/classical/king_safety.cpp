#include "eval/classical/king_safety.h"
#include "search/see.h"
#include <bit>

namespace {
constexpr int ATTACK_UNIT_PENALTY = 8;
constexpr int SHIELD_BONUS = 6;
constexpr int POCKET_THREAT_SCALE = 100;

int king_danger(const Board &board, const AttackInfo &attack_info,
                int board_idx, Colour us, const Pocket &enemy_pocket) {
  Colour them = flip(us);
  Bitboard zone = attack_info.kingZone[board_idx][us];
  Bitboard attacked = attack_info.attacks[board_idx][them] & zone;

  int danger = ATTACK_UNIT_PENALTY * std::popcount(attacked);

  Bitboard shield = board.bitboard_piece(make_piece(us, PAWN)) & zone;
  danger -= SHIELD_BONUS * std::popcount(shield);

  for (PieceType pt : {QUEEN, ROOK, BISHOP, KNIGHT})
    if (enemy_pocket.contains(pt))
      danger +=
          (SEE::PIECE_VALUE[pt] + SEE::POCKET_BONUS[pt]) / POCKET_THREAT_SCALE;

  return danger;
}
} // namespace

EvalScore KingSafetyEvaluator::evaluate(const EvalContext &context) const {
  int score = 0;
  for (int b = 0; b < BOARD_NO; b++) {
    const Board &board = context.position.boards[b];
    Colour ours = team_colour(context.search.root_player, b);
    Colour theirs = flip(ours);

    const Pocket &our_pocket =
        context.position.pockets[to_int(player_on_board(b, ours))];
    const Pocket &enemy_pocket =
        context.position.pockets[to_int(player_on_board(b, theirs))];

    int their_king_danger =
        king_danger(board, context.attack_info, b, theirs, our_pocket);
    int our_king_danger =
        king_danger(board, context.attack_info, b, ours, enemy_pocket);

    score += their_king_danger - our_king_danger;
  }
  return EvalScore(score);
}