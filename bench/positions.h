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
// position. This guarantees every position is legal according to
// Board::is_legal().
//
// Pockets are given as a string of piece letters (uppercase, one character
// per piece held), e.g. "PPN" = two pawns and one knight.
struct BenchPosition {
  std::string name;
  std::string description;

  std::vector<std::string> board0_moves;
  std::vector<std::string> board1_moves;

  // Optional escape hatch for positions that are impractical to reach via a
  // legal move sequence from the start position (e.g. hand-constructed
  // endgames). When non-empty, used instead of replaying board*_moves for
  // that board.
  std::string board0_fen;
  std::string board1_fen;

  std::array<std::string, PLAYER_NO> pockets;

  int root_player = 0;
};

// Builds a BughousePosition from a BenchPosition, replaying its move lists
// and populating its pockets. Throws std::runtime_error if a move in the
// suite does not correspond to a legal move at the point it is played
BughousePosition build_position(const BenchPosition &spec);

const std::vector<BenchPosition> &benchmark_suite();
