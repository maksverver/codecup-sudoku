#include "state.h"

#include <algorithm>
#include <bit>
#include <iostream>
#include <random>
#include <string>

std::string State::DebugString() const {
  std::string s(81, '.');
  for (int i = 0; i < 81; ++i) s[i] = digit[i] ? '0' + digit[i] : '.';
  return s;
}

void State::DebugPrint(std::ostream &os) const {
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      if (c > 0) os << ' ';
      char ch = '0' + digit[9*r + c];
      os << (ch == '0' ? '.' : ch);
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
  int max_used_count = -1;
  int max_used_index = -1;
  unsigned max_used_mask = 0;
  for (int j = 0; j < (int) todo.size(); ++j) {
    auto [i, r, c, b] = todo[j];
    unsigned used = used_row[r] | used_col[c] | used_box[b];
    if (used == ALL_DIGITS) return;  // unsolvable
    int used_count = std::popcount(used);
    if (used_count > max_used_count) {
      max_used_index = j;
      max_used_count = used_count;
      max_used_mask = used;
    }
  }
  std::swap(todo[max_used_index], todo.back());

  auto [i, r, c, b] = todo.back();
  std::span<Position> remaining(todo.begin(), todo.end() - 1);

  // Try all possible digits.
  unsigned unused = max_used_mask ^ ALL_DIGITS;
  while (unused && cs.count_left && cs.work_left) {
    --cs.work_left;

    unsigned mask = unused;
    unused &= unused - 1;
    mask ^= unused;

    used_row[r] ^= mask;
    used_col[c] ^= mask;
    used_box[b] ^= mask;

    CountSolutions(remaining, cs);

    used_row[r] ^= mask;
    used_col[c] ^= mask;
    used_box[b] ^= mask;
  }
}

EnumerateResult State::EnumerateSolutions(
    std::vector<std::array<uint8_t, 81>> &solutions,
    int max_count, int64_t max_work,
    std::mt19937 *rng) {
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
      unsigned unused = CellUsed(i) ^ ALL_DIGITS;
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
