#include "game/zobrist.h"
#include <mutex>
#include <random>

namespace Zobrist {
// piece × square
uint64_t pieceSquare[PIECE_NO][SQUARE_NO];
uint64_t side;
uint64_t castlingRights[CASTLING_RIGHTS_NO];
uint64_t enPassantFile[ENPASSANT_FILE_NO];
uint64_t pocket[PLAYER_NO][PIECE_TYPE_NO][MAX_POCKET_COUNT];

namespace {
std::once_flag init_flag;

void init() {
  std::mt19937_64 rng(0xAC0123456789ULL);
  for (int piece = 0; piece < PIECE_NO; piece++) {
    for (int square = 0; square < SQUARE_NO; square++) {
      pieceSquare[piece][square] = rng();
    }
  }

  side = rng();

  for (int cr = 0; cr < CASTLING_RIGHTS_NO; cr++) {
    castlingRights[cr] = rng();
  }

  for (int epf = 0; epf < ENPASSANT_FILE_NO; epf++) {
    enPassantFile[epf] = rng();
  }

  for (int p = 0; p < PLAYER_NO; p++)
    for (int pt = 0; pt < PIECE_TYPE_NO; pt++)
      for (int c = 0; c < MAX_POCKET_COUNT; c++)
        pocket[p][pt][c] = rng();
}
} // namespace

void ensure_init() { std::call_once(init_flag, init); }

} // namespace Zobrist