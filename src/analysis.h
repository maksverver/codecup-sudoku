#ifndef ANALYSIS_H
#define ANALYSIS_H

#include "state.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

// A grid is a 9x9 array where each value is between 0 and 9 (inclusive).
using grid_t = std::array<uint8_t, 81>;

// A solution is a grid where each value is between 1 and 9 (inclusive).
using solution_t = grid_t;

// A set of candidates is a bitmask of possible digits per field (note bit 0 is not used!)
using candidates_t = std::array<unsigned, 81>;

enum class Outcome {
  LOSS,
  WIN1,  // Immediately winning move detected
  WIN2,  // Winning, but not immediately.
  WIN3,  // Winning, by filling in an inferred digit
};

inline bool IsWinning(Outcome o) {
  return o == Outcome::WIN1 || o == Outcome::WIN2 || o == Outcome::WIN3;
}

std::ostream &operator<<(std::ostream &os, const Outcome &outcome);

struct AnalyzeResult {
  Outcome outcome;
  Move move;
};

std::ostream &operator<<(std::ostream &os, const AnalyzeResult &result);

// Given the set of given digits, and a *complete* set of solutions, determines
// the optimal move (first element of the result) and whether it is a winning.
//
// Preconditions: solutions.size() > 1
AnalyzeResult Analyze(const grid_t &givens, std::span<const solution_t> solutions);

#endif  // ndef ANALYSIS_H
