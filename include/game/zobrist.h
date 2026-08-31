#pragma once

#include "game/board.h"
#include "game/types.h"
#include <cstdint>

namespace Zobrist {

constexpr int CASTLING_RIGHTS_NO = 16;
constexpr int ENPASSANT_FILE_NO = 8;
constexpr int MAX_POCKET_COUNT = 21;

extern uint64_t pieceSquare[PIECE_NO][SQUARE_NO];
extern uint64_t side;
extern uint64_t castlingRights[];
extern uint64_t enPassantFile[ENPASSANT_FILE_NO];

// pocket[player][piece_type][count]
extern uint64_t pocket[PLAYER_NO][PIECE_TYPE_NO][MAX_POCKET_COUNT];

void ensure_init();

} // namespace Zobrist