#include "search/transposition_table.h"
#include <algorithm>

namespace {
size_t next_power_of_two(size_t n) {
  size_t p = 1;
  while (p < n)
    p <<= 1;
  return p;
}
} // namespace

TranspositionTable::TranspositionTable(size_t size_mb) { resize(size_mb); }

void TranspositionTable::resize(size_t size_mb) {
  size_t bytes = size_mb * 1024ULL * 1024ULL;
  size_t entries =
      next_power_of_two(std::max<size_t>(1, bytes / sizeof(TTEntry)));
  table_.assign(entries, TTEntry{});
  mask_ = entries - 1;
}

void TranspositionTable::clear() {
  std::fill(table_.begin(), table_.end(), TTEntry{});
  generation_ = 0;
}

void TranspositionTable::new_search() { generation_++; }

const TTEntry *TranspositionTable::probe(uint64_t key) const {
  const TTEntry &entry = table_[key & mask_];
  if (entry.depth >= 0 && entry.key == key)
    return &entry;
  return nullptr;
}

void TranspositionTable::store(uint64_t key, int depth, int score,
                               Move best_move, TTBound bound) {
  TTEntry &slot = table_[key & mask_];
  if (slot.depth < 0 || slot.generation != generation_ || depth >= slot.depth) {
    slot = TTEntry{
        key, static_cast<int16_t>(depth), score, best_move, bound, generation_};
  }
}