#pragma once

#include "game/types.h"

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

// bughouse/shared
constexpr int PARTNER_KING_DANGER_CLAMP = 20;

// bughouse/pocket
constexpr int POCKET_OPENNESS_MIDPOINT = 8; // 2 pts per open file, 8 files
constexpr int POCKET_OPENNESS_WEIGHT_MID[PIECE_TYPE_NO] = {0, 0, -3, 3, 4, 1, 0};
constexpr int POCKET_OPENNESS_WEIGHT_END[PIECE_TYPE_NO] = {0, 1, -1, 4, 5, 1, 0};
constexpr int POCKET_KING_ATTACK_WEIGHT_MID[PIECE_TYPE_NO] = {0, 1, 4, 3, 3, 6, 0};
constexpr int POCKET_KING_ATTACK_WEIGHT_END[PIECE_TYPE_NO] = {0, 2, 2, 2, 3, 5, 0};
constexpr int POCKET_PARTNER_CONFIDENCE_DIVISOR = 2;

// bughouse/drop
constexpr int DROP_CHECK_BONUS_MID = 25;
constexpr int DROP_CHECK_BONUS_END = 40;
constexpr int DROP_MATING_NET_BONUS_MID = 60;
constexpr int DROP_MATING_NET_BONUS_END = 90;
constexpr int DROP_FORK_WEIGHT_MID = 35;  // % of attacked material, mid game
constexpr int DROP_FORK_WEIGHT_END = 45;  // % of attacked material, end game
constexpr int DROP_PROMOTION_SUPPORT_MID = 15;
constexpr int DROP_PROMOTION_SUPPORT_END = 45;
constexpr int DROP_DEFENSE_BONUS_MID = 20;
constexpr int DROP_DEFENSE_BONUS_END = 15;
constexpr int DROP_KING_PROXIMITY_MID = 10;
constexpr int DROP_KING_PROXIMITY_END = 6;
constexpr int DROP_FLEXIBILITY_BONUS_MID = 8;
constexpr int DROP_FLEXIBILITY_BONUS_END = 5;
constexpr int DROP_PARTNER_ESTIMATE_WEIGHT_MID = 20;
constexpr int DROP_PARTNER_ESTIMATE_WEIGHT_END = 30;

// bughouse/exchange
constexpr float EXCHANGE_CONTESTED_FRACTION = 0.35f;
constexpr float EXCHANGE_BASE_MULTIPLIER = 1.0f;
constexpr float EXCHANGE_REQUEST_BONUS_MID = 0.4f;
constexpr float EXCHANGE_REQUEST_BONUS_END = 0.6f;
constexpr float EXCHANGE_PARTNER_HELP_BONUS = 0.25f;
constexpr float EXCHANGE_THREAT_DANGER_WEIGHT_MID = 1.5f;
constexpr float EXCHANGE_THREAT_DANGER_WEIGHT_END = 1.0f;
constexpr float EXCHANGE_THREAT_DANGER_FLAG_BONUS_MID = 0.6f;
constexpr float EXCHANGE_THREAT_DANGER_FLAG_BONUS_END = 0.4f;

// bughouse/communication
constexpr float COMM_URGENCY_WEIGHT[4] = {0.4f, 0.7f, 1.0f, 1.5f};
constexpr int COMM_ETA_HORIZON_PLIES = 6;
constexpr int COMM_REQUEST_FULFILLED_BONUS_MID = 30;
constexpr int COMM_REQUEST_FULFILLED_BONUS_END = 45;
constexpr int COMM_REQUEST_UNMET_PENALTY_MID = 18;
constexpr int COMM_REQUEST_UNMET_PENALTY_END = 10;
constexpr int COMM_PREDICTED_MATERIAL_WEIGHT_MID = 60; // % of predicted net swing, mid game
constexpr int COMM_PREDICTED_MATERIAL_WEIGHT_END = 80; // % of predicted net swing, end game
constexpr int COMM_ATTACK_READINESS_BONUS_MID = 25;
constexpr int COMM_ATTACK_READINESS_BONUS_END = 15;
constexpr int COMM_DEFENCE_READINESS_BONUS_MID = 15;
constexpr int COMM_DEFENCE_READINESS_BONUS_END = 25;
constexpr int COMM_STRATEGY_ALIGNMENT_BONUS_MID = 12;
constexpr int COMM_STRATEGY_ALIGNMENT_BONUS_END = 12;

// bughouse/king_danger
constexpr int KING_DANGER_CHECK_SQUARE_CAP = 4;
constexpr int KING_DANGER_WEIGHT_MID[PIECE_TYPE_NO] = {0, 6, 10, 8, 9, 14, 0};
constexpr int KING_DANGER_WEIGHT_END[PIECE_TYPE_NO] = {0, 4, 6, 6, 8, 10, 0};
constexpr float KING_DANGER_EXTRA_COPY_BONUS = 0.25f;
constexpr float KING_DANGER_BOX_WEIGHT_MID = 0.6f;
constexpr float KING_DANGER_BOX_WEIGHT_END = 0.3f;