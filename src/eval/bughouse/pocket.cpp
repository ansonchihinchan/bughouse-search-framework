#include "eval/bughouse/pocket.h"
#include "eval/const.h"
#include "game/bitboards.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>

namespace {

// fully-open file: +2, semi-open: +1, closed: +0
int file_openness(const Board &board) {
  Bitboard white_pawns = board.bitboard_piece(make_piece(WHITE, PAWN));
  Bitboard black_pawns = board.bitboard_piece(make_piece(BLACK, PAWN));

  int openness = 0;
  for (int file = 0; file < 8; file++) {
    Bitboard mask = Bitboards::file_mask(file);
    bool white_present = (white_pawns & mask) != 0;
    bool black_present = (black_pawns & mask) != 0;

    if (!white_present && !black_present)
      openness += 2;
    else if (!white_present || !black_present)
      openness += 1;
  }
  return openness;
}

// returns a raw count of attacks on the king's zone
int king_exposure(const ClassicalContext &context, Colour king_colour) {
  Bitboard zone = context.attack_info.kingZone[king_colour];
  Bitboard attacked = context.attack_info.attacks[flip(king_colour)] & zone;
  Bitboard shield =
      context.board.bitboard_piece(make_piece(king_colour, PAWN)) & zone;

  return std::max(0, std::popcount(attacked) - std::popcount(shield));
}

EvalScore drop_utility(PieceType pt, int count, int openness, int exposure) {
  if (count == 0)
    return EvalScore(0);

  int base = PieceValue::PIECE_VALUE[pt] + PieceValue::POCKET_BONUS[pt];
  int relative_openness = openness - POCKET_OPENNESS_MIDPOINT;

  int mid = base + POCKET_OPENNESS_WEIGHT_MID[pt] * relative_openness +
            POCKET_KING_ATTACK_WEIGHT_MID[pt] * exposure;
  int end = base + POCKET_OPENNESS_WEIGHT_END[pt] * relative_openness +
            POCKET_KING_ATTACK_WEIGHT_END[pt] * exposure;

  return EvalScore(mid * count, end * count);
}

EvalScore pocket_utility(const Pocket &pocket, int openness, int exposure) {
  EvalScore total(0);
  for (int pt = PAWN; pt <= QUEEN; pt++) {
    PieceType piece_type = static_cast<PieceType>(pt);
    total +=
        drop_utility(piece_type, pocket.count(piece_type), openness, exposure);
  }
  return total;
}

// plain material value with no board context at all
EvalScore base_material_utility(const Pocket &pocket) {
  EvalScore total(0);
  for (int pt = PAWN; pt <= QUEEN; pt++) {
    PieceType piece_type = static_cast<PieceType>(pt);
    int base = PieceValue::PIECE_VALUE[piece_type] +
               PieceValue::POCKET_BONUS[piece_type];
    total += EvalScore(base * pocket.count(piece_type));
  }
  return total;
}

} // namespace

EvalScore PocketEvaluator::evaluate(const EvalContext &context) const {
  const BughouseContext &bughouse = context.bughouse;
  Colour us = colour_of(bughouse.root_player);
  Colour them = flip(us);

  int openness = file_openness(context.classical.board);
  int their_exposure = king_exposure(context.classical, them);
  int our_exposure = king_exposure(context.classical, us);

  EvalScore score =
      pocket_utility(bughouse.own_pocket(), openness, their_exposure) -
      pocket_utility(bughouse.opp_pocket(), openness, our_exposure);

  if (!include_partner_pockets_)
    return score;

  const PartnerContext &partner = context.communication.partner;
  int partner_exposure = std::clamp(static_cast<int>(partner.king_danger), 0,
                                    PARTNER_KING_DANGER_CLAMP);

  EvalScore partner_raw = pocket_utility(
      bughouse.partner_pocket(), POCKET_OPENNESS_MIDPOINT, partner_exposure);
  EvalScore partner_score(
      partner_raw.mid_game() / POCKET_PARTNER_CONFIDENCE_DIVISOR,
      partner_raw.end_game() / POCKET_PARTNER_CONFIDENCE_DIVISOR);

  const Pocket &opp_partner_pocket =
      bughouse.pockets[to_int(partner_of(next_player(bughouse.root_player)))];

  EvalScore opp_partner_raw = base_material_utility(opp_partner_pocket);
  EvalScore opp_partner_score(
      opp_partner_raw.mid_game() / POCKET_PARTNER_CONFIDENCE_DIVISOR,
      opp_partner_raw.end_game() / POCKET_PARTNER_CONFIDENCE_DIVISOR);

  return score + partner_score - opp_partner_score;
}