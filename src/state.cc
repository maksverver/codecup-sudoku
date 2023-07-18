#include "state.h"

#include <iostream>
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

std::optional<std::pair<int, unsigned>> State::FindFree() const {
  int best_index;
  unsigned best_used;
  int max_used_count = -1;
  for (int i = 0; i < 81; ++i) if (digit[i] == 0) {
    unsigned used = CellUsed(i);
    int used_count = std::popcount(used);
    if (used_count > max_used_count) {
      best_index = i;
      best_used = used;
      max_used_count = used_count;
    }
  }
  if (max_used_count < 0) return {};
  return {{best_index, best_used}};
}

void State::CountSolutions(CountState &cs) {
  auto opt_free = FindFree();
  if (!opt_free) {
    --cs.count_left;
    return;
  }

  auto [i, used] = *opt_free;
  unsigned unused = used ^ 0b1111111110;
  while (unused && cs.count_left && cs.work_left) {
    --cs.work_left;

    unsigned mask = unused;
    unused &= unused - 1;
    mask ^= unused;

    digit[i] = -1;
    used_row[Row(i)] ^= mask;
    used_col[Col(i)] ^= mask;
    used_box[Box(i)] ^= mask;

    CountSolutions(cs);

    digit[i] = 0;
    used_row[Row(i)] ^= mask;
    used_col[Col(i)] ^= mask;
    used_box[Box(i)] ^= mask;
  }
}
