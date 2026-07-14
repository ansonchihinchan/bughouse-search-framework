#pragma once

#include "game/board.h"
#include "game/bughouse.h"
#include <array>
#include <string>
#include <vector>

// A single fixed point in the benchmark suite.
//
// Board positions are described as a sequence of UCI moves ("e2e4", "e7e8q",
// ...) applied to a fresh Board starting from the standard chess start
// position. This -- rather than hand-typed FEN strings -- guarantees every
// position is legal according to the engine's own move generator, since each
// move is validated with Board::is_legal() while the suite is built.
//
// Pockets are given as a string of piece letters (uppercase, one character
// per piece held), e.g. "PPN" = two pawns and one knight. King ('K') is
// never valid in a pocket. An empty string means an empty pocket.
struct BenchPosition {
  std::string name;
  std::string description;

  std::vector<std::string> board0_moves;
  std::vector<std::string> board1_moves;

  // Optional escape hatch for positions that are impractical to reach via a
  // legal move sequence from the start position (e.g. hand-constructed
  // endgames). When non-empty, used instead of replaying board*_moves for
  // that board. Small, hand-verifiable FENs only.
  std::string board0_fen;
  std::string board1_fen;

  // Indexed by PlayerId (0..3), matching BughousePosition::pockets.
  std::array<std::string, PLAYER_NO> pockets;

  // Whose perspective (and board) the search is run from.
  int root_player = 0;
};

// Builds a BughousePosition from a BenchPosition, replaying its move lists
// and populating its pockets. Throws std::runtime_error if a move in the
// suite does not correspond to a legal move at the point it is played --
// this is the suite's own self-check and should never fire for the shipped
// positions below.
BughousePosition build_position(const BenchPosition &spec);

// The permanent benchmark suite. Add new positions here; keep existing ones
// stable (or add new entries alongside them) so that benchmark results stay
// comparable across commits over time.
const std::vector<BenchPosition> &benchmark_suite();
