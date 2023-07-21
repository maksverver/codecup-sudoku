#include "analysis.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <optional>
#include <span>
#include <vector>

namespace {

// For each cell, calculates a bitmask of possible digits.
candidates_t CalculateCandidates(std::span<const solution_t> solutions) {
  candidates_t candidates = {};
  for (const solution_t &solution : solutions) {
    for (int i = 0; i < 81; ++i) candidates[i] |= 1u << solution[i];
  }
  return candidates;
}

bool Determined(unsigned mask) { return (mask & (mask - 1)) == 0; }

template<typename T> std::vector<T> Remove(const std::vector<T> &v, int i) {
  std::vector<T> res;
  res.reserve(v.size() - 1);
  for (const auto &j : v) if (j != i) res.push_back(j);
  return res;
}

// Let solutions be the subset {all_solutions[i] for all i in possibilites}.
//
// An immediately-winning move is a move that reduces the set of solutions to a
// single solution. i.e., it is a position and digit such that only one solution
// has that digit in that position.
std::optional<Move> FindImmediatelyWinningMove(
    std::span<const solution_t> solutions,
    std::span<const int> choice_positions) {
  assert(solutions.size() > 1);
  assert(choice_positions.size() > 0);
  for (int pos : choice_positions) {
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

// TODO later:
//  - optimize this with memoization (key = set of solutions)
//    - use a hash to memoize
//  - avoid unnecessary vector copying
//  - how to prevent timeout?
bool IsWinning(
    std::span<solution_t> solutions,
    std::span<const int> old_choice_positions) {
  assert(solutions.size() > 1);
  assert(!old_choice_positions.empty());

  candidates_t candidates = CalculateCandidates(solutions);
  std::vector<int> choice_positions;
  bool inferred_odd = false;
  for (int i : old_choice_positions) {
    if (Determined(candidates[i])) {
      inferred_odd = !inferred_odd;
    } else {
      choice_positions.push_back(i);
    }
  }

  if (auto result = FindImmediatelyWinningMove(solutions, choice_positions)) {
    return true;
  }

  for (int pos : choice_positions) {
    std::vector<int> new_choice_positions = Remove(choice_positions, pos);

    // TODO: maybe replace by a counting sort if this is a performance bottleneck
    std::ranges::sort(solutions, [pos](const solution_t &a, const solution_t &b) {
      return a[pos] < b[pos];
    });

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i][pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j][pos] == digit) ++j;
      // We should have found immediately-winning moves already before.
      assert(j - i > 1 && j - i < solutions.size());
      if (!IsWinning(solutions.subspan(i, j - i), new_choice_positions)) {
        return !inferred_odd;
      }
      i = j;
    }
  }

  return inferred_odd;
}

}  // namespace

std::pair<Move, bool> SelectMoveFromSolutions(
    const grid_t &givens, std::span<solution_t> solutions) {
  assert(solutions.size() > 1);

  candidates_t candidates = CalculateCandidates(solutions);
  std::vector<int> choice_positions;
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
    std::cerr << "Immediately winning move found!\n";
    return {*immediately_winning, true};
  }

  // Otherwise, if number of inferred moves is odd, pick one at random.
  if (inferred_moves.size() % 2 == 1) {
    std::cerr << "Inferred moves is odd.\n";
    return {RandomSample(inferred_moves), false};
  }

  // Otherwise, recursively search for a winning move.
  for (int pos : choice_positions) {
    std::vector<int> new_choice_positions = Remove(choice_positions, pos);

    std::ranges::sort(solutions, [pos](const solution_t &a, const solution_t &b) {
      return a[pos] < b[pos];
    });

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i][pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j][pos] == digit) ++j;
      // We should have found immediately-winning moves already before.
      assert(j - i > 1 && j - i < solutions.size());
      if (!IsWinning(solutions.subspan(i, j - i), new_choice_positions)) {
        // Losing for the next player => winning for the previous player.
        std::cerr << "Winning move found!\n";
        return {Move{.pos = pos, .digit = digit}, false};
      }
      i = j;
    }
  }

  std::cerr << "Losing :-(\n";
  return {RandomSample(inferred_moves), false};
}
