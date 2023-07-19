#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include <array>
#include <bit>
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

struct CountResult {
  int count = 0;
  int max_count = 2;
  int64_t work = 0;
  int64_t max_work = 1e18;

  bool Accurate() const { return work < max_work && count < max_count; }
  bool WorkLimitReached() const { return work >= max_work; }
  bool CountLimitReached() const { return count >= max_count; }
};

struct EnumerateResult {
  // `true` iff. the callback never returned false (including if it was never
  // called because there weren't any solutions.
  bool success;
  int64_t work = 0;
  int64_t max_work = 0;

  bool WorkLimitReached() const { return work >= max_work; }
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

  int Digit(int pos) const {
    assert(pos >= 0 && pos < 81);
    return digit[pos];
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

  CountResult CountSolutions(int max_count = 1e9, int64_t max_work = 1e18) {
    assert(max_count >= 0);
    assert(max_work >= 0);
    CountState state = {.count_left = max_count, .work_left = max_work};
    CountSolutions(state);
    assert(state.count_left >= 0);
    assert(state.work_left >= 0);
    return CountResult{
      .count = max_count - state.count_left,
      .max_count = max_count,
      .work = max_work - state.work_left,
      .max_work = max_work};
  }

  template<typename Callback>
  EnumerateResult EnumerateSolutions(const Callback &c, int64_t max_work = 1e18) {
    int64_t work_left = max_work;
    bool success = EnumerateSolutionsImpl(c, work_left);
    assert(work_left >= 0);
    return EnumerateResult{
      .success = success,
      .work = max_work - work_left,
      .max_work = max_work};
  }

  // Recursively fixes all cells that only have a single option left, and
  // returns number of cell values fixed this waty.
  int FixDetermined();

  std::string DebugString() const;

  void DebugPrint(std::ostream &os = std::cerr) const;

private:

  // Enumerates solutions and invokes callback(digits) until it returns
  // false, or until work_left is 0. Returns `false` if the callback ever returned
  // false, or true otherwise.
  template<typename C>
  bool EnumerateSolutionsImpl(const C &callback, int64_t &work_left) {
    auto opt_free = FindFree();
    if (!opt_free) return callback(const_cast<const uint8_t(&)[81]>(digit));

    auto [i, used] = *opt_free;
    unsigned unused = used ^ 0b1111111110;
    while (unused && work_left) {
      int d = std::countr_zero(unused);
      unused &= unused - 1;
      Move move = {i, d};
      Play(move);
      if (!EnumerateSolutionsImpl<C>(callback, work_left)) return false;
      Undo(move);
    }
    return true;
  }

  // Returns the index and the mask if used digits of the position where most
  // digits are already used (i.e., one of the most constrained cells).
  std::optional<std::pair<int, unsigned>> FindFree() const;

  struct CountState {
    int count_left = 2;
    int64_t work_left = 1e18;
  };

  // Recursively counts solutions.
  void CountSolutions(CountState &cs);

  uint8_t digit[81] = {};
  unsigned used_row[9] = {};
  unsigned used_col[9] = {};
  unsigned used_box[9] = {};
};

#endif // ndef STATE_H_INCLUDED
