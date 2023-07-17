#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

struct Move {
  int pos;
  int digit;

  void AssertValid() const {
    assert(0 <= pos && pos < 81);
    assert(1 <= digit && digit <= 9);
  }
};

inline int Row(int i) { return (unsigned) i / 9; }
inline int Col(int i) { return (unsigned) i % 9; }
inline int Blk(int i) { return ((unsigned) i % 9 / 3) + 3*((unsigned)i / 27); }

class State {
public:
  bool IsFree(int i) const { return digit[i] == 0; }

  unsigned CellUsed(int i) const {
    return used_row[Row(i)] | used_col[Col(i)] | used_blk[Blk(i)];
  }

  bool CanPlay(const Move &m) const {
    m.AssertValid();
    return digit[m.pos] == 0 && (CellUsed(m.pos) & (1u << m.digit)) == 0;
  }

  void Play(const Move &m) {
    digit[m.pos] = m.digit;
    unsigned mask = 1u << m.digit;
    used_row[Row(m.pos)] |= mask;
    used_col[Col(m.pos)] |= mask;
    used_blk[Blk(m.pos)] |= mask;
  }

  void Undo(const Move &m) {
    assert(digit[m.pos] == m.digit);
    digit[m.pos] = 0;
    unsigned mask = ~(1u << m.digit);
    used_row[Row(m.pos)] &= mask;
    used_col[Col(m.pos)] &= mask;
    used_blk[Blk(m.pos)] &= mask;
  }

  int CountSolutions(int64_t max_work = std::numeric_limits<decltype(max_work)>::max()) {
    int res = CountSolutionsRecursively(max_work);
    assert(max_work >= 0);
    if (max_work == 0) {
      std::cerr << "Work limit exceeded! " << DebugString() << std::endl;
    }
    return res;
  }

  std::string DebugString() const;

  void DebugPrint(std::ostream &os = std::cerr) const;

private:

  int CountSolutionsRecursively(int64_t &work) {
    auto opt_free = FindFree();
    if (!opt_free) return 1;

    int res = 0;
    auto [i, used] = *opt_free;
    for (int d = 1; d <= 9; ++d) if ((used & (1u << d)) == 0) {
      if (work == 0) return 2;
      --work;

      Move move = {i, d};
      Play(move);
      res += CountSolutionsRecursively(work);
      Undo(move);
      if (res >= 2) break;
    }
    return res;
  }

  // Returns the index and the mask if used digits of the position where most
  // digits are already used (i.e., one of the most constrained cells).
  std::optional<std::pair<int, unsigned>> FindFree() const {
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

  uint8_t digit[81] = {};
  unsigned used_row[9] = {};
  unsigned used_col[9] = {};
  unsigned used_blk[9] = {};
};

#endif // ndef STATE_H_INCLUDED
