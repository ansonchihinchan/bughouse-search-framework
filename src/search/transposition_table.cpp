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

TTReplacementPolicy default_policy() {
  return
      [](const TTEntry &current, const TTEntry &candidate, uint8_t generation) {
        return current.depth < 0 || current.generation != generation ||
               candidate.depth >= current.depth;
      };
}

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

void TranspositionTable::new_generation() { generation_++; }

void TranspositionTable::set_policy(TTReplacementPolicy policy) {
  policy_ = std::move(policy);
}

const TTEntry *TranspositionTable::probe(uint64_t key) const {
  const TTEntry &entry = table_[key & mask_];
  if (entry.depth >= 0 && entry.key == key)
    return &entry;
  return nullptr;
}

void TranspositionTable::store(uint64_t key, int depth, int score,
                               Move best_move, TTBound bound) {
  TTEntry &slot = table_[key & mask_];
  TTEntry candidate{
      key, static_cast<int16_t>(depth), score, best_move, bound, generation_};

  if (policy_(slot, candidate, generation_))
    slot = candidate;
}