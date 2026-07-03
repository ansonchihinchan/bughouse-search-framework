#pragma once
#include "board.h"
#include "pocket.h"
#include <vector>

// Returns all pseudo-legal moves
std::vector<Move> generate_moves(const Board &board,
                                 const Pocket *pocket = nullptr);

// Generate only drop moves from the given reserve
std::vector<Move> generate_drops(const Board &board, const Pocket &pocket);

// Count nodes at depth
uint64_t perft(Board &board, int depth, const Pocket *pocket = nullptr);