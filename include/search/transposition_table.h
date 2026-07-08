#pragma once

#include "game/types.h"
#include <cstddef>
#include <cstdint>
#include <vector>

enum class TTBound : uint8_t { EXACT, LOWER, UPPER };

struct TTEntry {
  uint64_t key = 0;
  int16_t depth = -1;
  int32_t score = 0;
  Move best_move;
  TTBound bound = TTBound::EXACT;
  uint8_t generation = 0;
};

class TranspositionTable {
public:
  explicit TranspositionTable(size_t size_mb = 64);

  void resize(size_t size_mb);
  void clear();
  void new_search();

  const TTEntry *probe(uint64_t key) const;
  void store(uint64_t key, int depth, int score, Move best_move, TTBound bound);

private:
  std::vector<TTEntry> table_;
  uint64_t mask_ = 0;
  uint8_t generation_ = 0;
};