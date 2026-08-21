#include "eval/bughouse/initiative.h"
#include "eval/const.h"
#include "game/attacks.h"

#include <algorithm>
#include <bit>

namespace {
int attackers_into_zone(const Board &board, Colour colour, Bitboard zone) {
  Bitboard occ = board.bitboard_all();
  int attackers = 0;
  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    Bitboard pieces = board.bitboard_piece(make_piece(colour, pt));
    while (pieces) {
      Square sq = static_cast<Square>(std::countr_zero(pieces));
      pieces &= pieces - 1;
      if (piece_attacks(pt, colour, sq, occ) & zone)
        attackers++;
    }
  }
  return attackers;
}

int king_escape_squares(const Board &board, Colour king_colour,
                        Square exclude_sq, Bitboard checking_attacks) {
  Bitboard king_bb = board.bitboard_piece(make_piece(king_colour, KING));
  if (!king_bb)
    return 8;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));
  Bitboard own_pieces = board.bitboard_colour(king_colour);
  Bitboard flight = king_attacks(ksq) & ~own_pieces & ~(1ULL << exclude_sq);
  return std::popcount(flight & ~checking_attacks);
}

Bitboard pinned_pieces(const Board &board, Colour colour) {
  Bitboard king_bb = board.bitboard_piece(make_piece(colour, KING));
  if (!king_bb)
    return 0;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  Colour enemy = flip(colour);
  Bitboard own = board.bitboard_colour(colour);
  Bitboard occ = board.bitboard_all();

  Bitboard diag_sliders = board.bitboard_piece(make_piece(enemy, BISHOP)) |
                          board.bitboard_piece(make_piece(enemy, QUEEN));
  Bitboard ortho_sliders = board.bitboard_piece(make_piece(enemy, ROOK)) |
                           board.bitboard_piece(make_piece(enemy, QUEEN));

  Bitboard pinned = 0;

  auto scan_dir = [&](int dir, Bitboard relevant_sliders) {
    Square first_blocker = -1;
    int cur = ksq + dir;
    while (cur >= 0 && cur < 64 &&
           std::abs((cur & 7) - ((cur - dir) & 7)) <= 1) {
      Bitboard bit = 1ULL << cur;
      if (occ & bit) {
        if (first_blocker == -1) {
          if (own & bit)
            first_blocker = static_cast<Square>(cur);
          else
            return;
        } else {
          if (relevant_sliders & bit)
            pinned |= 1ULL << first_blocker;
          return;
        }
      }
      cur += dir;
    }
  };

  for (int dir : DIAG_DIRS)
    scan_dir(dir, diag_sliders);
  for (int dir : ORTHO_DIRS)
    scan_dir(dir, ortho_sliders);

  return pinned;
}

struct CheckSquares {
  Bitboard knight = 0;
  Bitboard diagonal = 0;
  Bitboard orthogonal = 0;
  Bitboard pawn = 0;
};

CheckSquares compute_check_squares(const Board &board, Colour king_colour,
                                   Bitboard occ) {
  CheckSquares squares;
  Bitboard king_bb = board.bitboard_piece(make_piece(king_colour, KING));
  if (!king_bb)
    return squares;
  Square ksq = static_cast<Square>(std::countr_zero(king_bb));

  squares.knight = knight_attacks(ksq);
  squares.diagonal = bishop_attacks(ksq, occ);
  squares.orthogonal = rook_attacks(ksq, occ);
  squares.pawn = pawn_attacks(ksq, king_colour);

  return squares;
}

Bitboard check_squares_for(const CheckSquares &squares, PieceType pt) {
  switch (pt) {
  case PAWN:
    return squares.pawn;
  case KNIGHT:
    return squares.knight;
  case BISHOP:
    return squares.diagonal;
  case ROOK:
    return squares.orthogonal;
  case QUEEN:
    return squares.diagonal | squares.orthogonal;
  default:
    return 0;
  }
}

struct CheckSurvey {
  int check_move_count = 0;
  int tightest_escape_squares = 8;
  bool any_check_found = false;
};

CheckSurvey survey_board_checks(const Board &board, Colour side) {
  CheckSurvey survey;

  Bitboard occ = board.bitboard_all();
  Bitboard own = board.bitboard_colour(side);
  Bitboard pinned = pinned_pieces(board, side);
  Bitboard enemy_king_bb = board.bitboard_piece(make_piece(flip(side), KING));

  CheckSquares check_sqs = compute_check_squares(board, flip(side), occ);

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    Bitboard target_mask = check_squares_for(check_sqs, pt);
    if (!target_mask)
      continue;

    Bitboard pieces = board.bitboard_piece(make_piece(side, pt)) & ~pinned;
    while (pieces) {
      Square from = static_cast<Square>(std::countr_zero(pieces));
      pieces &= pieces - 1;

      Bitboard reach = piece_attacks(pt, side, from, occ) & ~own;
      Bitboard checks = reach & target_mask;

      while (checks) {
        Square to = static_cast<Square>(std::countr_zero(checks));
        checks &= checks - 1;

        survey.check_move_count++;
        survey.any_check_found = true;

        Bitboard occ_after = (occ & ~(1ULL << from)) | (1ULL << to);
        Bitboard attacks_from_to = piece_attacks(pt, side, to, occ_after);
        int escapes =
            king_escape_squares(board, flip(side), to, attacks_from_to);
        survey.tightest_escape_squares =
            std::min(survey.tightest_escape_squares, escapes);
      }
    }
  }

  Bitboard promo_rank =
      (side == WHITE) ? 0x00FF000000000000ULL : 0x000000000000FF00ULL;
  Bitboard promoting_pawns =
      board.bitboard_piece(make_piece(side, PAWN)) & ~pinned & promo_rank;

  while (promoting_pawns) {
    Square from = static_cast<Square>(std::countr_zero(promoting_pawns));
    promoting_pawns &= promoting_pawns - 1;

    Square push_to = static_cast<Square>(from + (side == WHITE ? 8 : -8));
    if (occ & (1ULL << push_to))
      continue; // blocked, no push-promotion available

    Bitboard occ_after = (occ & ~(1ULL << from)) | (1ULL << push_to);

    for (PieceType promo_pt : {QUEEN, KNIGHT}) {
      Bitboard promo_attacks =
          piece_attacks(promo_pt, side, push_to, occ_after);
      if (!(promo_attacks & enemy_king_bb))
        continue;

      survey.check_move_count++;
      survey.any_check_found = true;

      int escapes =
          king_escape_squares(board, flip(side), push_to, promo_attacks);
      survey.tightest_escape_squares =
          std::min(survey.tightest_escape_squares, escapes);
    }
  }

  return survey;
}

int drop_check_readiness(const Board &board, const Pocket &pocket,
                         Colour mover) {
  if (pocket.empty())
    return 0;

  DropCheckMasks checks = drop_check_masks(board, mover);
  int ready_types = 0;

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    if (!pocket.contains(pt))
      continue;

    if (checks.for_piece(pt))
      ready_types++;
  }
  return ready_types;
}

EvalScore momentum_score(const Board &board, const AttackInfo &attack_info,
                         Colour attacker) {
  Bitboard zone = attack_info.kingZone[flip(attacker)];
  int reach = std::popcount(attack_info.attacks[attacker] & zone);
  int attackers = attackers_into_zone(board, attacker, zone);

  int mid = INITIATIVE_ATTACKER_UNIT_MID * reach;
  int end = INITIATIVE_ATTACKER_UNIT_END * reach;

  if (attackers >= 2) {
    mid += INITIATIVE_MULTI_ATTACKER_BONUS_MID;
    end += INITIATIVE_MULTI_ATTACKER_BONUS_END;
  }

  return EvalScore(mid, end);
}

EvalScore checking_score(const Board &board, Colour side) {
  if (board.sideToMove != side)
    return EvalScore(0);

  CheckSurvey survey = survey_board_checks(board, side);
  if (!survey.any_check_found)
    return EvalScore(0);

  int counted = std::min(survey.check_move_count, INITIATIVE_CHECK_MOVE_CAP);
  int mid = INITIATIVE_CHECK_MOVE_MID * counted;
  int end = INITIATIVE_CHECK_MOVE_END * counted;

  int net_tightness = std::max(0, 4 - survey.tightest_escape_squares);
  mid += INITIATIVE_FORCING_CHAIN_BONUS_MID * net_tightness / 4;
  end += INITIATIVE_FORCING_CHAIN_BONUS_END * net_tightness / 4;

  return EvalScore(mid, end);
}

EvalScore drop_readiness_score(const Board &board, const Pocket &pocket,
                               Colour mover) {
  int ready_types = drop_check_readiness(board, pocket, mover);
  if (ready_types == 0)
    return EvalScore(0);

  return EvalScore(INITIATIVE_DROP_CHECK_READY_MID * ready_types,
                   INITIATIVE_DROP_CHECK_READY_END * ready_types);
}

} // namespace

EvalScore InitiativeEvaluator::evaluate(const EvalContext &context) const {
  const ClassicalContext &classical = context.classical;
  const BughouseContext &bughouse = context.bughouse;
  const Board &board = classical.board;

  Colour us = colour_of(bughouse.root_player);
  Colour them = flip(us);

  EvalScore score(0);

  // Attack momentum
  score += momentum_score(board, classical.attack_info, us);
  score -= momentum_score(board, classical.attack_info, them);

  // Checking opportunities + forcing sequence potential
  score += checking_score(board, us);
  score -= checking_score(board, them);

  // Attacking tempo
  if (board.is_in_check()) {
    if (board.sideToMove == them)
      score +=
          EvalScore(INITIATIVE_TEMPO_BONUS_MID, INITIATIVE_TEMPO_BONUS_END);
    else
      score -=
          EvalScore(INITIATIVE_TEMPO_BONUS_MID, INITIATIVE_TEMPO_BONUS_END);
  }

  // Reserve checking material
  score += drop_readiness_score(board, bughouse.own_pocket(), us);
  score -= drop_readiness_score(board, bughouse.opp_pocket(), them);

  return score;
}