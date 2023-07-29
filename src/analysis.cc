#include "analysis.h"
#include "counters.h"
#include "memo.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace {

using position_t = int_fast8_t;

memo_t memo;

struct HashedSolution {
  memo_key_t hash;
  solution_t solution;
};

constexpr uint64_t Fnv1a_64(std::span<const uint8_t> bytes) {
  uint64_t hash = 0xcbf29ce484222325;
  for (uint8_t byte : bytes) {
    hash ^= byte;
    hash *= 0x100000001b3;
  }
  return hash;
}

memo_key_t Hash(const solution_t &solution) {
  static_assert(std::is_same<memo_key_t, uint64_t>::value);
  return Fnv1a_64(solution);
}

// Note: this is an order-independent hash! All permutations of solutions
// have the same hash value. That's intentional, since the span of solutions
// is supposed to model a set of solutions, not an ordered sequence.
memo_key_t HashSolutionSet(std::span<const HashedSolution> solutions) {
  memo_key_t hash = 0;
  for (const auto &entry : solutions) hash ^= entry.hash;
  return hash;
}

// For each cell, calculates a bitmask of possible digits.
candidates_t CalculateCandidates(std::span<const solution_t> solutions) {
  candidates_t candidates = {};
  for (const solution_t &solution : solutions) {
    for (int i = 0; i < 81; ++i) candidates[i] |= 1u << solution[i];
  }
  return candidates;
}

constexpr bool Determined(unsigned mask) { return (mask & (mask - 1)) == 0; }

// Returns how many times it would make sense to place an inferred digit.
//
// Currently I believe it only makes sense to try placing an inferred digit when
// the count is odd, since otherwise, each inferred digit filled in could be
// countered by the opponent by filling in another inferred digit, which doesn't
// change the value of the position.
//
// The reason that this is a separate function is that it allows testing the
// hypothesis that only the parity of `inferred_count` matters, by changing the
// implementation into `return inferred_count;` In that case, analysis would run
// slower, but should give the same results.
constexpr int ReduceInferredCount(int inferred_count) {
  return inferred_count & 1;
}

constexpr memo_key_t HashInferredCount(int inferred_count) {
  const memo_key_t key = 0x2ac473eb0dac37ae;  // randomly generated
  return key * ReduceInferredCount(inferred_count);
}

template<typename T> std::vector<T> Remove(std::span<const T> v, T i) {
  std::vector<T> res;
  res.reserve(v.size() - 1);
  for (const auto &j : v) if (j != i) res.push_back(j);
  return res;
}

// Rearranges the span of solutions into three parts:
//
//  Part 1: solution[pos] < A
//  Part 2: A <= solution[pos] < B
//  Part 3: solution[pos] >= B
//
// Returns two iterators: the beginning and end of part 2.
template <int A, int B>
std::pair<std::span<HashedSolution>::iterator, std::span<HashedSolution>::iterator>
DutchNationalFlagSort(std::span<HashedSolution> solutions, position_t pos) {
  auto mid_begin = solutions.begin();
  auto mid_end = solutions.end();
  for (auto it = mid_begin; it != mid_end; ) {
    if (it->solution[pos] < A) {
      if (it != mid_begin) std::swap(*it, *mid_begin);
      ++mid_begin;
      ++it;
    } else if (it->solution[pos] < B) {
      ++it;
    } else {
      --mid_end;
      if (it != mid_end) std::swap(*it, *mid_end);
    }
  }
  return {mid_begin, mid_end};
}

void SortByDigitAtPosition(std::span<HashedSolution> solutions, position_t pos) {
  auto [mid_begin, mid_end] = DutchNationalFlagSort<4, 7>(solutions, pos);
  DutchNationalFlagSort<2, 3>({solutions.begin(), mid_begin}, pos);
  DutchNationalFlagSort<5, 6>({mid_begin, mid_end}, pos);
  DutchNationalFlagSort<8, 9>({mid_end, solutions.end()}, pos);

#if 0
  assert(std::ranges::is_sorted(solutions,
      [pos](const HashedSolution &a, const HashedSolution &b) {
        return a.solution[pos] < b.solution[pos];
      }));
#endif
}

// Let solutions be the subset {all_solutions[i] for all i in possibilites}.
//
// An immediately-winning move is a move that reduces the set of solutions to a
// single solution. i.e., it is a position and digit such that only one solution
// has that digit in that position.
std::optional<Move> FindImmediatelyWinningMove(
    std::span<const solution_t> solutions,
    std::span<const position_t> choice_positions) {
  assert(solutions.size() > 1);
  assert(choice_positions.size() > 0);
  for (position_t pos : choice_positions) {
    int solution_count[10] = {};
    for (const solution_t &solution : solutions) {
      ++solution_count[solution[pos]];
    }
    for (int digit = 1; digit <= 9; ++digit) {
      if (solution_count[digit] == 1) {
        return Move{pos, digit};
      }
    }
  }
  return {};
}

// Recursively solves the given state assuming that:
//  - there are at least two solutions left,
//  - no immediately winning moves exist, and
//  - all single-candidate cells have been removed from choice_positions.
//
// This function is mutually recursive with IsWinning() (defined below) and
// its results are memoized in IsWinning().
bool IsWinning2(
    std::span<HashedSolution> solutions,
    std::span<position_t> choice_positions,
    int inferred_count,
    int depth);

// This function determines if the given state is winning for the next player.
//
//  1. Remove inferred digits.
//  2. Search for immediately winning positions.
//  3. Call IsWinning2() to solve the reduced grid without inferred digits.
//
// Step 3 is memoized to avoid duplicate work. Note that step 1 and 2 are
// not memoized, since the return value depends on the parity of the number
// of inferred digits.
bool IsWinning(
    std::span<HashedSolution> solutions,
    std::span<const position_t> old_choice_positions,
    int inferred_count,
    int depth) {
  assert(solutions.size() > 1);
  assert(!old_choice_positions.empty());

  // Update counters.
  counters.max_depth.SetMax(depth);
  counters.recusive_calls.Inc();
  counters.total_solutions.Add(solutions.size());

  // Calculate new choice positions and detect immediately winning moves.
  //
  // For each choice position:
  //
  //  1. Check if is has only 1 possible digit across all solutions. If so, this
  //     is an inferred digit and we omit it from the new choice positions.
  //  2. Check if there is a digit that occurs in exactly 1 solution. If so,
  //     then this is an immediately winning move.
  //
  position_t choice_positions_data[81];
  size_t choice_positions_size = 0;
  for (position_t pos : old_choice_positions) {
    int solution_count[9] = {};
    bool inferred = false;
    for (const auto &entry : solutions) {
      if (++solution_count[entry.solution[pos] - 1] == (int) solutions.size()) {
        ++inferred_count;
        inferred = true;
        break;
      }
    }
    if (!inferred) {
      for (int c : solution_count) if (c == 1) {
        // Immediately winning!
        counters.immediately_won.Inc();
        return true;
      }
      choice_positions_data[choice_positions_size++] = pos;
    }
  }

  bool winning;

  // Memoization happens here.
  counters.memo_accessed.Inc();
  memo_key_t key = HashSolutionSet(solutions) ^ HashInferredCount(inferred_count);
  auto mem = memo.Lookup(key);
  if (mem.HasValue()) {
    counters.memo_returned.Inc();
    winning = mem.GetWinning();
  } else {
    // Solve recursively.
    std::span<position_t> choice_positions(choice_positions_data, choice_positions_size);
    winning = IsWinning2(solutions, choice_positions, inferred_count, depth);
    mem.SetWinning(winning);
  }

  return winning;
}

bool IsWinning2(
    std::span<HashedSolution> solutions,
    std::span<position_t> choice_positions,
    int inferred_count,
    int depth) {

  for (size_t k = 0; k < choice_positions.size(); ++k) {
    position_t pos = choice_positions[k];

    // Temporarily replace selected position.
    choice_positions[k] = choice_positions[choice_positions.size() - 1];

    SortByDigitAtPosition(solutions, pos);

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i].solution[pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j].solution[pos] == digit) ++j;
      // We should have found immediately-winning moves already before.
      assert(j - i > 1 && j - i < solutions.size());
      if (!IsWinning(
              solutions.subspan(i, j - i),
              choice_positions.subspan(0, choice_positions.size() - 1),
              inferred_count, depth + 1)) {
        // Winning move found!
        return true;
      }
      i = j;
    }

    // Restore temporarily replaced position.
    choice_positions[k] = pos;
  }

  if (ReduceInferredCount(inferred_count) > 0) {
    bool winning = IsWinning(solutions, choice_positions, inferred_count - 1, depth + 1);
    if (!winning) return true;
  }

  return false;
}

// Recursively solves the given state assuming that:
//
//  - there are at least two solutions left,
//  - no immediately winning moves exist,
//  - inferred_moves have been removed from choice_positions.
//
// This is very similar to IsWinning2() except this also returns an optimal
// move to play.
Move SelectMoveFromSolutions2(
    const std::vector<position_t> &choice_positions,
    const std::vector<Move> &inferred_moves,
    std::span<HashedSolution> solutions) {
  assert(solutions.size() > 1);

  // Recursively search for a winning move.
  std::vector<Move> losing_moves = inferred_moves;
  int max_solutions_remaining = inferred_moves.empty() ? 0 : solutions.size();
  for (position_t pos : choice_positions) {
    std::vector<position_t> new_choice_positions =
        Remove<position_t>(choice_positions, pos);

    SortByDigitAtPosition(solutions, pos);

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i].solution[pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j].solution[pos] == digit) ++j;
      size_t n = j - i;
      // We should have found immediately-winning moves already before.
      assert(n > 1 && n < solutions.size());
      Move move = {.pos = pos, .digit = digit};
      if (IsWinning(solutions.subspan(i, n), new_choice_positions, inferred_moves.size(), 1)) {
        // Winning for the next player => losing for the previous player.
        if ((int) n > max_solutions_remaining) {
          max_solutions_remaining = n;
          losing_moves.clear();
        }
        if ((int) n == max_solutions_remaining) {
          losing_moves.push_back(move);
        }
      } else {
        // Losing for the next player => winning for the previous player.
        std::cerr << "Winning move found!\n";
        return move;
      }
      i = j;
    }
  }

  if (ReduceInferredCount(inferred_moves.size()) > 0) {
    if (!IsWinning(solutions, choice_positions, inferred_moves.size() - 1, 1)) {
      std::cerr << "Winning move found! (using an odd inferred move)\n";
      return RandomSample(inferred_moves);
    }
  }

  std::cerr << "No winning move found :-(\n";
  return RandomSample(losing_moves);
}

}  // namespace

std::pair<Move, bool> SelectMoveFromSolutions(
    const grid_t &givens, std::span<const solution_t> solutions) {
  assert(solutions.size() > 1);

  counters.recusive_calls.Inc();
  counters.total_solutions.Add(solutions.size());

  candidates_t candidates = CalculateCandidates(solutions);
  std::vector<position_t> choice_positions;
  std::vector<Move> inferred_moves;
  for (int i = 0; i < 81; ++i) {
    if (givens[i] == 0) {
      if (Determined(candidates[i])) {
        int digit = std::countr_zero(candidates[i]);
        inferred_moves.push_back(Move{.pos = i, .digit = digit});
      } else {
        choice_positions.push_back(i);
      }
    }
  }

  // If there is an immediately winning move, always take it!
  if (auto immediately_winning = FindImmediatelyWinningMove(solutions, choice_positions)) {
    std::cerr << "That's numberwang! (Immediately winning move found.)\n";
    return {*immediately_winning, true};
  }

  std::vector<HashedSolution> hashed_solutions;
  hashed_solutions.reserve(solutions.size());
  for (const auto &solution : solutions) {
    hashed_solutions.push_back(HashedSolution{Hash(solution), solution});
  }

  // Otherwise, recursively search for a winning move.
  Move move = SelectMoveFromSolutions2(choice_positions, inferred_moves, hashed_solutions);

  // Note: we could clear the memo here to save memory, but keeping it populated
  // will help with future searches especially in the common case where both
  // players fill in an inferred digit, which doesn't change the analysis at all.

  return {move, false};
}
