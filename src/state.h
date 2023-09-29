#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include "random.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

// Set to 0 to not maximize the number of solutions that remain
// when making a losing move.
#define MAXIMIZE_SOLUTIONS_REMAINING 1

struct Move {
  int pos;
  int digit;

  void AssertValid() const {
    assert(0 <= pos && pos < 81);
    assert(1 <= digit && digit <= 9);
  }
};

// During a turn, a player can either play a single move, or play 0, 1 or 2
// moves and claim that the solution is unique, which ends the game.
struct Turn {
  bool claim_unique;
  int move_count;
  Move moves[1];

  Turn(const Turn &t) = default;
  Turn &operator=(const Turn &t) = default;

  // Default state which isn't a valid turn.
  bool Empty() const { return claim_unique == false && move_count == 0; }

  std::span<const Move> Moves() const { return std::span<const Move>(moves, move_count); }

  explicit Turn(bool claim_unique = false) : claim_unique(claim_unique) {}
  explicit Turn(const Move &move, bool claim_unique = false) : claim_unique(claim_unique){
    moves[0] = move;
    move_count = 1;
  }
};

std::ostream &operator<<(std::ostream &os, const Move &move);
std::ostream &operator<<(std::ostream &os, const Turn &turn);

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
  bool success = false;
  int64_t work = 0;
  int64_t max_work = 0;

  bool Accurate() const { return success && work < max_work; }
  bool WorkLimitReached() const { return work >= max_work; }
};

class State {

public:

  bool IsFree(int i) const { return digit[i] == 0; }

  unsigned CellUnused(int i) const {
    return unused_row[Row(i)] & unused_col[Col(i)] & unused_box[Box(i)];
  }

  bool CanPlay(const Move &m) const {
    m.AssertValid();
    return digit[m.pos] == 0 && (CellUnused(m.pos) & (1u << m.digit)) != 0;
  }

  int Digit(int pos) const {
    assert(pos >= 0 && pos < 81);
    return digit[pos];
  }

  void Play(const Move &m) {
    assert(digit[m.pos] == 0);
    digit[m.pos] = m.digit;
    unsigned mask = 1u << m.digit;
    unused_row[Row(m.pos)] ^= mask;
    unused_col[Col(m.pos)] ^= mask;
    unused_box[Box(m.pos)] ^= mask;
  }

  void Undo(const Move &m) {
    assert(digit[m.pos] == m.digit);
    digit[m.pos] = 0;
    unsigned mask = ~(1u << m.digit);
    unused_row[Row(m.pos)] ^= mask;
    unused_col[Col(m.pos)] ^= mask;
    unused_box[Box(m.pos)] ^= mask;
  }

  struct Position { uint8_t i, r, c, b; };

  std::span<Position> GetEmptyPositions(std::array<Position, 81> &buf) {
    int n = 0;
    for (uint8_t r = 0; r < 9; ++r) {
      for (uint8_t c = 0; c < 9; ++c) {
        uint8_t i = 9*r + c;
        if (digit[i] == 0) {
          uint8_t b = 3*(r / 3) + (c / 3);
          buf[n++] = Position{.i = i, .r = r, .c = c, .b = b};
        }
      }
    }
    return std::span<Position>(buf.data(), n);
  }

  CountResult CountSolutions(int max_count = 1e9, int64_t max_work = 1e18) {
    assert(max_count >= 0);
    assert(max_work >= 0);
    CountState state = {.count_left = max_count, .work_left = max_work};
    std::array<Position, 81> buf;
    std::span<Position> todo = GetEmptyPositions(buf);
    CountSolutions(todo, state);
    assert(state.count_left >= 0);
    assert(state.work_left >= 0);
    return CountResult{
      .count = max_count - state.count_left,
      .max_count = max_count,
      .work = max_work - state.work_left,
      .max_work = max_work};
  }

  // Enumerates up to `max_count` solutions and stores them in the given vector.
  // (The vector is cleared at the start.)
  EnumerateResult EnumerateSolutions(
    std::vector<std::array<uint8_t, 81>> &solutions,
    int max_count = 1e9, int64_t max_work = 1e18,
    rng_t *rng = nullptr);

  // Enumerates solutions and invokes callback(digits) until it returns false,
  // or until work_left is 0. Returns `false` if the callback ever returned
  // false, or true otherwise.
  template<typename Callback>
  EnumerateResult EnumerateSolutions(
      const Callback &callback, int64_t max_work = 1e18, rng_t *rng = nullptr) {
    std::array<Position, 81> buf;
    std::span<Position> todo = GetEmptyPositions(buf);
    if (rng) std::shuffle(todo.begin(), todo.end(), *rng);
    int64_t work_left = max_work;
    bool success = EnumerateSolutionsImpl(callback, todo, work_left);
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

  // Note: the logic here is very similar to CountSolutions().
  template<typename C>
  bool EnumerateSolutionsImpl(const C &callback, std::span<Position> todo, int64_t &work_left) {
    if (todo.empty()) {
      // Solution found!
      return callback(const_cast<const std::array<uint8_t, 81>&>(digit));
    }

    // Find most constrained cell to fill in.
    int min_unused_count = 10;
    int min_unused_index = -1;
    unsigned min_unused_mask = 0;
    for (int j = 0; j < (int) todo.size(); ++j) {
      auto [i, r, c, b] = todo[j];
      unsigned unused = unused_row[r] & unused_col[c] & unused_box[b];
      if (unused == 0) return true;  // unsolvable
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
    while (unused && work_left) {
      --work_left;

      int d = std::countr_zero(unused);
      digit[i] = d;

      unsigned mask = 1 << d;
      unused ^= mask;

      unused_row[r] ^= mask;
      unused_col[c] ^= mask;
      unused_box[b] ^= mask;

      bool result = EnumerateSolutionsImpl<C>(callback, remaining, work_left);

      unused_row[r] ^= mask;
      unused_col[c] ^= mask;
      unused_box[b] ^= mask;

      digit[i] = 0;

      if (!result) return false;
    }
    return true;
  }

  struct CountState {
    int count_left = 2;
    int64_t work_left = 1e18;
  };

  // Recursively counts solutions.
  void CountSolutions(std::span<Position> todo, CountState &cs);

  std::array<uint8_t, 81> digit = {};
  static constexpr unsigned A = 0b1111111110;  // all-digit bitmask
  unsigned unused_row[9] = {A, A, A, A, A, A, A, A, A};
  unsigned unused_col[9] = {A, A, A, A, A, A, A, A, A};
  unsigned unused_box[9] = {A, A, A, A, A, A, A, A, A};
};

#endif // ndef STATE_H_INCLUDED
