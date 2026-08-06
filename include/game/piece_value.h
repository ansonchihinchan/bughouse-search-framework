#pragma once

#include "game/types.h"
#include <array>

namespace PieceValue {
inline constexpr std::array<int, PIECE_TYPE_NO> PIECE_VALUE{0,   100, 320,  330,
                                                            550, 900, 20000};
inline constexpr std::array<int, PIECE_TYPE_NO> POCKET_BONUS{0,  40,  60, 60,
                                                             90, 150, 0};
inline constexpr std::array<int, PIECE_TYPE_NO> PHASE_WEIGHT{0, 0, 1, 1,
                                                             2, 4, 0};
} // namespace PieceValue