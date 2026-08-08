#include "eval/classical/king_safety.h"
#include "eval/const.h"
#include <bit>

namespace {
int king_danger(const Board &board, const AttackInfo &attack_info, Colour us) {
  Colour them = flip(us);
  Bitboard zone = attack_info.kingZone[us];
  Bitboard attacked = attack_info.attacks[them] & zone;

  int danger = ATTACK_UNIT_PENALTY * std::popcount(attacked);

  Bitboard shield = board.bitboard_piece(make_piece(us, PAWN)) & zone;
  danger -= SHIELD_BONUS * std::popcount(shield);

  return danger;
}
} // namespace

EvalScore KingSafetyEvaluator::evaluate(const ClassicalContext &context) const {
  const Board &board = context.board;

  return EvalScore(king_danger(board, context.attack_info, BLACK) -
                   king_danger(board, context.attack_info, WHITE));
}