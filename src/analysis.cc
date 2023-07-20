#include "analysis.h"

#include "random.h"
#include "state.h"

#include <optional>
#include <vector>

namespace {

// For each cell, calculates a bitmask of possible digits.
candidates_t CalculateCandidates(
    const solutions_t &all_solutions,
    const std::vector<size_t> &possibilities) {
  candidates_t candidates = {};
  for (size_t i : possibilities) {
    for (int j = 0; j < 81; ++j) candidates[j] |= 1u << all_solutions[i][j];
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

std::vector<size_t> NarrowPossibilities(
    const solutions_t &all_solutions,
    const std::vector<size_t> &possibilities,
    int pos, int digit) {
  std::vector<size_t> remaining;
  for (size_t i : possibilities) {
    if (all_solutions[i][pos] == digit) {
      remaining.push_back(i);
    }
  }
  return remaining;
}

// Let solutions be the subset {all_solutions[i] for all i in possibilites}.
//
// An immediately-winning move is a move that reduces the set of solutions to a
// single solution. i.e., it is a position and digit such that only one solution
// has that digit in that position.
std::optional<Move> FindImmediatelyWinningMove(
    const solutions_t &all_solutions,
    const std::vector<size_t> &possibilities,
    const std::vector<int> &choice_positions) {
  assert(possibilities.size() > 1);
  assert(choice_positions.size() > 0);
  // TODO: this could be optimized by only checking the position that we know are still different
  for (int pos : choice_positions) {
    int solution_count[10] = {};
    for (auto i : possibilities) {
      ++solution_count[all_solutions[i][pos]];
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
    const solutions_t &all_solutions,
    const std::vector<size_t> &possibilities,
    const std::vector<int> &old_choice_positions,
    int depth) {
  assert(possibilities.size() > 1);
  assert(!old_choice_positions.empty());

  candidates_t candidates = CalculateCandidates(all_solutions, possibilities);
  std::vector<int> choice_positions;
  bool inferred_odd = false;
  for (int i : old_choice_positions) {
    if (Determined(candidates[i])) {
      inferred_odd = !inferred_odd;
    } else {
      choice_positions.push_back(i);
    }
  }

  if (auto result = FindImmediatelyWinningMove(all_solutions, possibilities, choice_positions)) {
    return true;
  }

  for (int pos : choice_positions) {
    std::vector<int> new_choice_positions = Remove(choice_positions, pos);

    for (int digit = 1; digit <= 9; ++digit) if ((candidates[pos] & (1u << digit)) != 0) {
      std::vector<size_t> new_possibilities = NarrowPossibilities(all_solutions, possibilities, pos, digit);

      // We should have found immediately-winning moves already before.
      assert(new_possibilities.size() > 1 && new_possibilities.size() < possibilities.size());

      if (!IsWinning(all_solutions, new_possibilities, new_choice_positions, depth + 1)) {
        return !inferred_odd;
      }
    }
  }

  return inferred_odd;
}

}  // namespace

std::pair<Move, bool> SelectMoveFromSolutions(
    const grid_t &givens, const solutions_t &solutions) {
  assert(solutions.size() > 1);

  std::vector<size_t> possibilities(solutions.size());
  std::iota(possibilities.begin(), possibilities.end(), 0);
  candidates_t candidates = CalculateCandidates(solutions, possibilities);
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
  if (auto immediately_winning = FindImmediatelyWinningMove(solutions, possibilities, choice_positions)) {
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

    for (int digit = 1; digit <= 9; ++digit) if (candidates[pos] & (1u << digit)) {
      std::vector<size_t> new_possibilities = NarrowPossibilities(solutions, possibilities, pos, digit);

      // We should have found immediately-winning moves already before.
      assert(new_possibilities.size() > 1);

      if (!IsWinning(solutions, new_possibilities, new_choice_positions, 0)) {
        // Losing for the next player => winning for the previous player.
        std::cerr << "Winning move found!\n";
        return {Move{.pos = pos, .digit = digit}, false};
      }
    }
  }

  std::cerr << "Losing :-(\n";
  return {RandomSample(inferred_moves), false};
}
