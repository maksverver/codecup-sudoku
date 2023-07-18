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
inline int Box(int i) { return ((unsigned) i % 9 / 3) + 3*((unsigned)i / 27); }

struct CountState {
  int count = 0;
  int max_count = 2;
  int64_t max_work = 1e18;

  bool WorkLimitExceeded() const { return max_work < 0; }
  bool CountLimitReached() const { return count >= max_count; }
};

class State {
public:
  bool IsFree(int i) const { return digit[i] == 0; }

  unsigned CellUsed(int i) const {
    return used_row[Row(i)] | used_col[Col(i)] | used_box[Box(i)];
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
    used_box[Box(m.pos)] |= mask;
  }

  void Undo(const Move &m) {
    assert(digit[m.pos] == m.digit);
    digit[m.pos] = 0;
    unsigned mask = ~(1u << m.digit);
    used_row[Row(m.pos)] &= mask;
    used_col[Col(m.pos)] &= mask;
    used_box[Box(m.pos)] &= mask;
  }

  CountState CountSolutions(int max_count = 1e9, int64_t max_work = 1e18) {
    assert(max_count >= 0);
    assert(max_work >= 0);
    CountState state = {.max_count = max_count, .max_work = max_work};
    CountSolutions(state);
    assert(state.count <= state.max_count);
    assert(state.max_work >= 0);
    return state;
  }

  std::string DebugString() const;

  void DebugPrint(std::ostream &os = std::cerr) const;

private:
  // Returns the index and the mask if used digits of the position where most
  // digits are already used (i.e., one of the most constrained cells).
  std::optional<std::pair<int, unsigned>> FindFree() const;

  // Recursively counts solutions.
  void CountSolutions(CountState &cs);

  uint8_t digit[81] = {};
  unsigned used_row[9] = {};
  unsigned used_col[9] = {};
  unsigned used_box[9] = {};
};

#endif // ndef STATE_H_INCLUDED
