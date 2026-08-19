#include "communication/generator.h"
#include "game/attacks.h"
#include "game/piece_value.h"

#include <algorithm>
#include <bit>

Message Generator::generate_message(const SearchResult &search_result,
                                    const BughousePosition &position,
                                    PlayerId root_player) {
  Message message;
  message.sender = root_player;

  const Board &board = position.boards[board_of(root_player)];
  const Pocket &pocket = position.pockets[to_int(root_player)];
  message.move_no = board.fullMove;

  DropCheckMasks checks = drop_check_masks(board, colour_of(root_player));
  PieceType best_piece = NO_PIECE_TYPE;
  int best_square_count = 0;
  int best_value = 0;

  for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN}) {
    if (pocket.contains(pt))
      continue;

    int square_count = std::popcount(checks.for_piece(pt));
    int value = PieceValue::effective_value(pt);
    if (square_count > best_square_count ||
        (square_count == best_square_count && square_count > 0 &&
         value > best_value)) {
      best_piece = pt;
      best_square_count = square_count;
      best_value = value;
    }
  }

  if (best_piece != NO_PIECE_TYPE) {
    message.piece_request.piece = best_piece;
    message.piece_request.confidence =
        std::min(1.0f, static_cast<float>(best_square_count) / 4.0f);
    message.piece_request.urgency =
        board.is_in_check() ? Urgency::Critical
        : best_square_count >= 4 ? Urgency::High
                                 : Urgency::Medium;
    message.piece_request.eta_plies = 0;
  }

  message.strat_request.strat =
      search_result.score < 0 ? StrategyType::Defend : StrategyType::AttackNow;
  message.strat_request.confidence = 0.5f;
  message.strat_request.urgency =
      board.is_in_check() ? Urgency::Critical : Urgency::Medium;

  return message;
}