#pragma once

#include "game/board.h"

// --- Shared ---

constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
inline Bitboard file_mask(int file) { return FILE_A_BB << file; }

// --- Weights ---

// classical/activity
constexpr int UNDEVELOPED_PENALTY = 15;
constexpr int OPEN_FILE_BONUS = 15;
constexpr int SEMI_OPEN_FILE_BONUS = 8;

// classical/king_safety
constexpr int ATTACK_UNIT_PENALTY = 8;
constexpr int SHIELD_BONUS = 6;

// classical/mobility
constexpr int MOBILITY_WEIGHT[PIECE_TYPE_NO] = {0, 0, 4, 3, 2, 1, 0};

// classical/pawn
constexpr int PASSED_BONUS = 20;
constexpr int ISOLATED_PENALTY = 15;
constexpr int DOUBLED_PENALTY = 10;

// classical/space
// White: ranks 2-3, Black: ranks 6-7
constexpr Bitboard WHITE_SPACE_RANKS = 0x0000000000FFFF00ULL;
constexpr Bitboard BLACK_SPACE_RANKS = 0x00FFFF0000000000ULL;
constexpr int SPACE_WEIGHT = 2;

// classical/tempo
constexpr int TEMPO_BONUS = 12;