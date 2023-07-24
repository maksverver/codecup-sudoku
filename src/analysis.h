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

struct AnalysisStats {
    int depth = 0, max_depth = 0;
    int64_t recusive_calls = 0;
    int64_t total_solutions = 0;
    int64_t immediately_won = 0;
    int64_t memo_accessed = 0;
    int64_t memo_returned = 0;
};

std::ostream &operator<<(std::ostream &os, const AnalysisStats &stats);

// Given the set of given digits, and a *complete* set of solutions, determines
// the optimal move (first element of the result) and whether it is a winning
// immediately (second element of the result).
//
// Preconditions: solutions.size() > 1
std::pair<Move, bool> SelectMoveFromSolutions(
    const grid_t &givens, std::span<const solution_t> solutions, AnalysisStats *stats);

#endif  // ndef ANALYSIS_H
