#include "analysis.h"
#include "memo.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace {

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
candidates_t CalculateCandidates(std::span<const HashedSolution> solutions) {
  candidates_t candidates = {};
  for (auto const &entry : solutions) {
    for (int i = 0; i < 81; ++i) candidates[i] |= 1u << entry.solution[i];
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

template<typename T> std::vector<T> Remove(std::span<const T> v, int i) {
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
    std::span<const HashedSolution> solutions,
    std::span<const int> choice_positions) {
  assert(solutions.size() > 1);
  assert(choice_positions.size() > 0);
  for (int pos : choice_positions) {
    int solution_count[10] = {};
    for (const auto &entry : solutions) {
      ++solution_count[entry.solution[pos]];
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
    std::span<const int> choice_positions,
    int inferred_count,
    AnalysisStats *stats);

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
    std::span<const int> old_choice_positions,
    int inferred_count,
    AnalysisStats *stats) {
  assert(solutions.size() > 1);
  assert(!old_choice_positions.empty());

  if (stats) {
    stats->max_depth = std::max(stats->max_depth, stats->depth);
    stats->recusive_calls += 1;
    stats->total_solutions += solutions.size();
  }

  candidates_t candidates = CalculateCandidates(solutions);
  int choice_positions_data[81];
  size_t choice_positions_size = 0;
  for (int i : old_choice_positions) {
    if (Determined(candidates[i])) {
      ++inferred_count;
    } else {
      choice_positions_data[choice_positions_size++] = i;
    }
  }
  std::span<const int> choice_positions(choice_positions_data, choice_positions_size);

  if (auto result = FindImmediatelyWinningMove(solutions, choice_positions)) {
    if (stats) ++stats->immediately_won;
    return true;
  }

  bool winning;

  // Memoization happens here.
  if (stats) ++stats->memo_accessed;
  memo_key_t key = HashSolutionSet(solutions) ^ HashInferredCount(inferred_count);
  auto mem = memo.Lookup(key);
  if (mem.HasValue()) {
    if (stats) ++stats->memo_returned;
    winning = mem.GetWinning();
  } else {
    // Solve recursively.
    winning = IsWinning2(solutions, choice_positions, inferred_count, stats);
    mem.SetWinning(winning);
  }

  return winning;
}

bool IsWinning2(
    std::span<HashedSolution> solutions,
    std::span<const int> choice_positions,
    int inferred_count,
    AnalysisStats *stats) {

  if (ReduceInferredCount(inferred_count) > 0) {
    if (stats) ++stats->depth;
    bool winning = IsWinning(solutions, choice_positions, inferred_count - 1, stats);
    if (stats) --stats->depth;
    if (!winning) return true;
  }

  for (int pos : choice_positions) {
    std::vector<int> new_choice_positions = Remove(choice_positions, pos);

    // TODO: maybe replace by a counting sort if this is a performance bottleneck
    std::ranges::sort(solutions, [pos](const HashedSolution &a, const HashedSolution &b) {
      return a.solution[pos] < b.solution[pos];
    });

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i].solution[pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j].solution[pos] == digit) ++j;
      // We should have found immediately-winning moves already before.
      assert(j - i > 1 && j - i < solutions.size());
      if (stats) ++stats->depth;
      bool winning = IsWinning(solutions.subspan(i, j - i), new_choice_positions, inferred_count, stats);
      if (stats) --stats->depth;
      if (!winning) return true;
      i = j;
    }
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
    const std::vector<int> &choice_positions,
    const std::vector<Move> &inferred_moves,
    std::span<HashedSolution> solutions,
    AnalysisStats *stats) {
  assert(solutions.size() > 1);

  // Recursively search for a winning move.
  std::vector<Move> losing_moves = inferred_moves;
  int max_solutions_remaining = inferred_moves.empty() ? 0 : solutions.size();
  if (ReduceInferredCount(inferred_moves.size()) > 0) {
    if (!IsWinning(solutions, choice_positions, inferred_moves.size() - 1, stats)) {
      std::cerr << "Winning move found! (using an odd inferred move)\n";
      return RandomSample(inferred_moves);
    }
  }
  for (int pos : choice_positions) {
    std::vector<int> new_choice_positions = Remove<int>(choice_positions, pos);

    std::ranges::sort(solutions, [pos](const HashedSolution &a, const HashedSolution &b) {
      return a.solution[pos] < b.solution[pos];
    });

    size_t i = 0;
    while (i < solutions.size()) {
      int digit = solutions[i].solution[pos];
      size_t j = i + 1;
      while (j < solutions.size() && solutions[j].solution[pos] == digit) ++j;
      size_t n = j - i;
      // We should have found immediately-winning moves already before.
      assert(n > 1 && n < solutions.size());
      Move move = {.pos = pos, .digit = digit};
      if (IsWinning(solutions.subspan(i, n), new_choice_positions, inferred_moves.size(), stats)) {
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

  std::cerr << "No winning move found :-(\n";
  return RandomSample(losing_moves);
}

}  // namespace

std::ostream &operator<<(std::ostream &os, const AnalysisStats &stats) {
  assert(stats.depth == 0);

  return os << "AnalysisStats{\n"
    << "\tmax_depth=" << stats.max_depth << ",\n"
    << "\trecusive_calls=" << stats.recusive_calls << ",\n"
    << "\ttotal_solutions=" << stats.total_solutions << ",\n"
    << "\timmediately_won=" << stats.immediately_won << ",\n"
    << "\tmemo_accessed=" << stats.memo_accessed << ",\n"
    << "\tmemo_returned=" << stats.memo_returned << "}";
}

std::pair<Move, bool> SelectMoveFromSolutions(
    const grid_t &givens, std::span<const solution_t> solutions, AnalysisStats *stats) {
  assert(solutions.size() > 1);

  if (stats) {
    stats->recusive_calls += 1;
    stats->total_solutions += solutions.size();
  }

  std::vector<HashedSolution> hashed_solutions;
  hashed_solutions.reserve(solutions.size());
  for (const auto &solution : solutions) {
    hashed_solutions.push_back(HashedSolution{Hash(solution), solution});
  }

  candidates_t candidates = CalculateCandidates(hashed_solutions);
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
  if (auto immediately_winning = FindImmediatelyWinningMove(hashed_solutions, choice_positions)) {
    std::cerr << "That's numberwang! (Immediately winning move found.)\n";
    return {*immediately_winning, true};
  }

  // Otherwise, recursively search for a winning move.
  Move move = SelectMoveFromSolutions2(choice_positions, inferred_moves, hashed_solutions, stats);

  // Note: we could clear the memo here to save memory, but keeping it populated
  // will help with future searches especially in the common case where both
  // players fill in an inferred digit, which doesn't change the analysis at all.

  return {move, false};
}
