#pragma once

#include "game/bughouse.h"
#include "game/types.h"
#include <vector>

class Board;
class Pocket;
struct BughouseState;

// Board level
std::vector<Move> generate_pseudo_legal_moves(const Board &board,
                                              const Pocket *pocket = nullptr);

// Bughouse level
std::vector<Move> generate_legal_moves(const BughousePosition &position,
                                       PlayerId player_id);

// Generate only drop moves from the given reserve
std::vector<Move> generate_drop_moves(const Board &board, const Pocket &pocket);

// Count nodes at depth
uint64_t perft(Board &board, int depth, const Pocket *pocket = nullptr);