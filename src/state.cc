#include "state.h"

#include <algorithm>
#include <bit>
#include <iostream>
#include <random>
#include <string>

static char Char(int d, char zero='.') {
  return d == 0 ? zero : (char) ('0' + d);
}

std::ostream &operator<<(std::ostream &os, const Move &move) {
  return os << "Move{pos=" << move.pos << ", digit=" << move.digit << "}";
}

std::string State::DebugString() const {
  std::string s(81, '.');
  for (int i = 0; i < 81; ++i) s[i] = Char(digit[i]);
  return s;
}

void State::DebugPrint(std::ostream &os) const {
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      if (c > 0) os << ' ';
      os << Char(digit[9*r + c]);
    }
    os << '\n';
  }
  os << '\n';
}

// Note: the logic here is very similar to EnumerateSolutionsImpl(), except
// that this version never actually fills in any digits.
void State::CountSolutions(std::span<Position> todo, CountState &cs) {
  if (todo.empty()) {
    // Solution found!
    --cs.count_left;
    return;
  }

  // Find most constrained cell to fill in.
  int min_unused_count = 10;
  int min_unused_index = -1;
  unsigned min_unused_mask = 0;
  for (int j = 0; j < (int) todo.size(); ++j) {
    auto [i, r, c, b] = todo[j];
    unsigned unused = unused_row[r] & unused_col[c] & unused_box[b];
    if (unused == 0) return;  // unsolvable
    int unused_count = std::popcount(unused);
    if (unused_count < min_unused_count) {
      min_unused_index = j;
      min_unused_count = unused_count;
      min_unused_mask = unused;
    }
  }
  std::swap(todo[min_unused_index], todo.back());

  auto [i, r, c, b] = todo.back();
  std::span<Position> remaining(todo.begin(), todo.end() - 1);

  // Try all possible digits.
  unsigned unused = min_unused_mask;
  while (unused && cs.count_left && cs.work_left) {
    --cs.work_left;

    unsigned mask = unused;
    unused &= unused - 1;
    mask ^= unused;

    unused_row[r] ^= mask;
    unused_col[c] ^= mask;
    unused_box[b] ^= mask;

    CountSolutions(remaining, cs);

    unused_row[r] ^= mask;
    unused_col[c] ^= mask;
    unused_box[b] ^= mask;
  }
}

EnumerateResult State::EnumerateSolutions(
    std::vector<std::array<uint8_t, 81>> &solutions,
    int max_count, int64_t max_work,
    rng_t *rng) {
  assert(max_count >= 0);
  solutions.clear();
  return EnumerateSolutions(
    [&solutions, max_count](const std::array<uint8_t, 81> &digits){
      solutions.push_back(digits);
      return solutions.size() < (size_t) max_count;
    },
    max_work,
    rng);
}

// This is currently unused!
int State::FixDetermined() {
  int fixed = 0;
  for (;;) {
    int pos[81];
    int val[81];
    int n = 0;
    for (int i = 0; i < 81; ++i) if (digit[i] == 0) {
      unsigned unused = CellUnused(i);
      assert(unused != 0);
      if ((unused & (unused - 1)) == 0) {
        pos[n] = i;
        val[n] = std::countr_zero(unused);
        ++n;
      }
    }
    if (n == 0) return fixed;
    for (int i = 0; i < n; ++i) Play(Move{.pos = pos[i], .digit=val[i]});
    fixed += n;
  }
}
