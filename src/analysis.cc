#include "analysis.h"
#include "counters.h"
#include "memo.h"
#include "state.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <random>
#include <span>
#include <vector>

// Enable to collect detailed statistics.
#define COLLECT_STATS 0

namespace {


#if COLLECT_STATS

constexpr int percentiles[] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100};

std::vector<int> stats_solutions;
std::vector<int> stats_positions;
std::vector<int> stats_moves;
std::vector<int> stats_winning_positions;
std::vector<int> stats_winning_solutions;

void DumpStats(std::vector<int> &stats) {
  assert(!stats.empty());
  std::cout << std::setw(9) << stats.size()
      << std::fixed << std::setw(6) << std::setprecision(1)
      << 1.0*std::accumulate(stats.begin(), stats.end(), uint64_t{0}) / stats.size();
  std::ranges::sort(stats);
  for (int percentile : percentiles) {
    std::cout << std::setw(6) << stats[(uint64_t{stats.size()} - 1) * percentile / 100];
  }
}

void DumpStats() {
  std::cout << "            " << std::setw(9) << "samples" << std::setw(6) << "avg.";
  for (int percentile : percentiles) {
    std::cout << std::setw(5) << percentile << '%';
  }
  std::cout << "\nwin. sols   "; DumpStats(stats_winning_solutions);
  std::cout << "\nrem. sols   "; DumpStats(stats_solutions);
  std::cout << "\nrem. posits "; DumpStats(stats_positions);
  std::cout << "\nrem. moves  "; DumpStats(stats_moves);
  std::cout << std::endl;
}

#endif // COLLECT_STATS

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

template<typename T> std::vector<T> Remove(std::span<const T> v, T i) {
  std::vector<T> res;
  res.reserve(v.size() - 1);
  for (const auto &j : v) if (j != i) res.push_back(j);
  return res;
}

// std::span<T> wrapper that allows iteration in sorted order.
//
// Construction takes O(N) time, dereferencing an iterator takes O(1) time
// and incrementing an iterator takes O(log N) time.
//
// This is useful in the case where it is unlikely that the entire range
// is iterated over.
//
// Note that the elements of the span are reordered during iteration!
template<class T> class SortingIterable {
  struct Iterator {
    std::span<T>::iterator begin, end;

    T &operator*() { return *begin; }

    Iterator &operator++() {
      std::pop_heap(begin, end, std::greater<T>());
      --end;
      return *this;
    }

    bool operator==(const Iterator &o) const { return end == o.end; }
  };

public:
  explicit SortingIterable(std::span<T> data) : data(data) {};

  Iterator begin() {
    std::make_heap(data.begin(), data.end(), std::greater<T>());
    return Iterator{data.begin(), data.end()};
  }

  Iterator end() {
    return Iterator{data.begin(), data.begin()};
  }

private:

  std::span<T> data;
};

/*

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

*/

std::span<HashedSolution> FilterSolutions(std::span<HashedSolution> solutions, Move move) {
  return std::span<HashedSolution>(
    solutions.begin(),
    std::partition(solutions.begin(), solutions.end(),
        [move](const auto &s) { return s.solution[move.pos] == move.digit; }));
}

std::span<position_t> FilterPositions(std::span<position_t> positions, position_t pos) {
  for (position_t &p : positions) {
    if (p == pos) {
      std::swap(p, positions.back());
      return positions.subspan(0, positions.size() - 1);
    }
  }
  assert(false);
  return positions;
}

constexpr int max_moves = 9 * 9 * 9;

struct RankedMove {
  Move move;
  int solution_count;

  auto operator<=>(const RankedMove &o) const { return solution_count <=> o.solution_count; }
};

// Generates stably sorted ranked moves, sorted by increasing solution count.
// This may include immediately winning moves with solution count == 1, which
// will necessarily appear at the front of the list.
std::vector<RankedMove> GenerateRankedMoves(
    std::span<HashedSolution> solutions,
    std::span<const position_t> choice_positions) {
  std::vector<RankedMove> moves;
  for (position_t pos : choice_positions) {
    int solution_count[9] = {};
    for (const auto &entry : solutions) {
      ++solution_count[entry.solution[pos] - 1];
    }
    for (int digit = 1; digit <= 9; ++digit) {
      int n = solution_count[digit - 1];
      if (n > 0) {
        assert((size_t) n < solutions.size());
        moves.push_back(RankedMove{
              .move = Move{.pos = pos, .digit = digit},
              .solution_count = n,
            });
      }
    }
  }
  std::stable_sort(moves.begin(), moves.end());
  return moves;
}

// This function determines if the given state is winning for the next player.
bool IsWinning(
    std::span<HashedSolution> solutions,
    std::span<const position_t> old_choice_positions,
    int depth,
    int64_t &work_left) {
  assert(solutions.size() > 1);
  assert(!old_choice_positions.empty());

  // Update counters.
  counters.max_depth.SetMax(depth);
  counters.recursive_calls.Inc();
  counters.total_solutions.Add(solutions.size());

  work_left -= solutions.size();
  if (work_left < 0) return false;  // Search aborted.

  // Check memo for cached result.
  counters.memo_accessed.Inc();
  memo_key_t key = HashSolutionSet(solutions);
  auto mem = memo.Lookup(key);
  if (mem.HasValue()) {
    counters.memo_returned.Inc();
    return mem.GetWinning();
  }

  // Calculate new choice positions and detect immediately winning moves.
  //
  // For each choice position:
  //
  //  1. Check if is has only 1 possible digit across all solutions. If so, this
  //     is an inferred digit and we omit it from the new choice positions.
  //  2. Check if there is a digit that occurs in exactly 1 solution. If so,
  //     then this is an immediately winning move.
  int solution_counts[81][9] = {};
  position_t choice_positions_data[81];
  size_t choice_positions_size = 0;
  for (position_t pos : old_choice_positions) {
    bool inferred = false;
    for (const auto &entry : solutions) {
      if (++solution_counts[pos][entry.solution[pos] - 1] == (int) solutions.size()) {
        inferred = true;
        break;
      }
    }
    if (!inferred) {
      for (int c : solution_counts[pos]) if (c == 1) {
        // Immediately winning!
        counters.immediately_won.Inc();
        mem.SetWinning(true);
#if COLLECT_STATS
        stats_winning_solutions.push_back(solutions.size());
#endif
        return true;
      }
      choice_positions_data[choice_positions_size++] = pos;
    }
  }

  std::span<position_t> choice_positions(choice_positions_data, choice_positions_size);

  RankedMove moves_data[max_moves];
  size_t moves_size = 0;
  for (position_t pos : choice_positions) {
    for (int digit = 1; digit <= 9; ++digit) {
      int solution_count = solution_counts[pos][digit - 1];
      if (solution_count > 0) {
        moves_data[moves_size++] = RankedMove{
          .move = Move{.pos = pos, .digit = digit},
          .solution_count = solution_count,
        };
      }
    }
  }

#if COLLECT_STATS
  stats_solutions.push_back(solutions.size());
  stats_positions.push_back(choice_positions_size);
  stats_moves.push_back(moves_size);
#endif

  // Solve recursively. We consider all possible moves: if there is a move
  // that leads to a position that is losing for the opponent, then that
  // move is winning for the current player.
  bool winning = false;
  std::span<RankedMove> moves(moves_data, moves_size);
  for (const auto [move, solution_count] : SortingIterable(moves)) {
    bool next_losing = !IsWinning(
        FilterSolutions(solutions, move),
        FilterPositions(choice_positions, move.pos),
        depth + 1, work_left);
    if (work_left < 0) return false;  // Search aborted.
    if (next_losing) { winning = true; break; }
  }
  mem.SetWinning(winning);
  return winning;
}

std::vector<Turn> Turns(std::span<const Move> moves, bool claim_unique=false) {
  std::vector<Turn> result;
  result.reserve(moves.size());
  for (const Move &move : moves) result.push_back(Turn{move, claim_unique});
  return result;
}

// Recursively solves the given state assuming that:
//
//  - there are at least two solutions left,
//  - no immediately winning moves exist,
//
// This is very similar to IsWinning2() except this also returns an optimal
// move to play.
AnalyzeResult SelectMoveFromSolutions2(
    std::span<HashedSolution> solutions,
    std::vector<position_t> &choice_positions,
    const std::vector<RankedMove> &ranked_moves,
    int max_winning_turns,
    int64_t work_left) {
  assert(solutions.size() > 1);

  // Recursively search for a winning move.
  std::vector<Turn> losing_turns;
  std::vector<Turn> winning_turns;
#if MAXIMIZE_SOLUTIONS_REMAINING
  size_t max_solutions_remaining = 0;
#endif

  for (const auto &[move, solution_count] : ranked_moves) {
    auto remaining_solutions = FilterSolutions(solutions, move);
    auto remaining_choice_positions = FilterPositions(choice_positions, move.pos);
    // We should have found immediately-winning moves already before.
    assert(remaining_solutions.size() == (size_t) solution_count &&
        solution_count > 1 && (size_t) solution_count < solutions.size());
    bool winning = IsWinning(remaining_solutions, remaining_choice_positions, 1, work_left);
    if (work_left < 0) return AnalyzeResult{};  // Search aborted.
    if (winning) {
      // Winning for the next player => losing for the previous player.
#if MAXIMIZE_SOLUTIONS_REMAINING
      if (remaining_solutions.size() > max_solutions_remaining) {
          max_solutions_remaining = remaining_solutions.size();
          losing_turns.clear();
        }
      if (remaining_solutions.size() == max_solutions_remaining) {
        losing_turns.push_back(Turn(move));
      }
#else
      losing_turns.push_back(Turn(move));
#endif
    } else {
      // Losing for the next player => winning for the previous player.
      winning_turns.push_back(Turn(move));
      if (winning_turns.size() >= (size_t) max_winning_turns) goto max_winning_turns_found;
    }
  }

  if (!winning_turns.empty()) {
    // Technicaly it's possible that playing an inferred move is also winning,
    // but since I can only return one outcome, I will drop those moves if there
    // is a winning move that reduces the number of solutions.
max_winning_turns_found:
    return AnalyzeResult{Outcome::WIN2, winning_turns};
  }

  return AnalyzeResult{Outcome::LOSS, losing_turns};
}

}  // namespace

std::ostream &operator<<(std::ostream &os, const Outcome &outcome) {
  switch (outcome) {
  case Outcome::LOSS: return os << "LOSS";
  case Outcome::WIN1: return os << "WIN1";
  case Outcome::WIN2: return os << "WIN2";
  default:
    assert(false);
    return os;
  }
}

std::ostream &operator<<(std::ostream &os, const AnalyzeResult &result) {
  os << "AnalyzeResult{outcome=";
  if (result.outcome) {
    os << *result.outcome;
  } else {
    os << "<unknown>";
  }
  os << ", optimal_turns={";
  bool first = true;
  for (const Turn &turn : result.optimal_turns) {
    if (first) first = false; else os << ", ";
    os << turn;
  }
  os << "}}";
  return os;
}

AnalyzeResult Analyze(
    const grid_t &givens, std::span<const solution_t> solutions,
    int max_winning_turns, int64_t max_work) {
  assert(!solutions.empty());
  assert(max_winning_turns > 0);

  if (solutions.size() == 1) {
    // Solution is already unique.
    return AnalyzeResult{Outcome::WIN1, {Turn(true)}};
  }

  counters.recursive_calls.Inc();
  counters.total_solutions.Add(solutions.size());

  candidates_t candidates = CalculateCandidates(solutions);
  std::vector<position_t> choice_positions;
  for (int i = 0; i < 81; ++i) {
    if (givens[i] == 0) {
      if (!Determined(candidates[i])) {
        choice_positions.push_back(i);
      }
    }
  }

  std::vector<HashedSolution> hashed_solutions;
  hashed_solutions.reserve(solutions.size());
  for (const auto &solution : solutions) {
    hashed_solutions.push_back(HashedSolution{Hash(solution), solution});
  }

  std::vector<RankedMove> ranked_moves = GenerateRankedMoves(hashed_solutions, choice_positions);
  assert(!ranked_moves.empty());

  // If there is an immediately winning move, always take it!
  if (ranked_moves.front().solution_count == 1) {
    std::vector<Move> immediately_winning;
    for (auto [move, solution_count] : ranked_moves) {
      if (solution_count != 1) break;
      immediately_winning.push_back(move);
    }
    return AnalyzeResult{Outcome::WIN1, Turns(immediately_winning, true)};
  }

  // Otherwise, recursively search for a winning move.
  auto res = SelectMoveFromSolutions2(
      hashed_solutions, choice_positions, ranked_moves, max_winning_turns,
      max_work - solutions.size());

  // Note: we could clear the memo before returning to save memory, but keeping
  // it populated will help with future searches especially in the common case
  // where both players fill in an inferred digit, which doesn't change the
  // analysis.

#if COLLECT_STATS
  DumpStats();
#endif

  return res;
}
