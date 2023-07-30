#include "analysis.h"
#include "counters.h"
#include "state.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

constexpr int max_count = 1000000;

char Char(int d, char zero='.') {
  assert(d >= 0 && d < 10);
  return d == 0 ? zero : (char) ('0' + d);
}

std::optional<State> ParseDesc(const char *desc) {
  State state;
  for (int i = 0; i < 81; ++i) {
    if (desc[i] >= '1' && desc[i] <= '9') {
      Move move = {i, desc[i] - '0'};
      if (!state.CanPlay(move)) {
        std::cerr << "Invalid given at index " << i << ": " << desc[i] << std::endl;
        return {};
      }
      state.Play(move);
    } else if (desc[i] != '.' && desc[i] != '0') {
      std::cerr << "Invalid character at index " << i << ": " << desc[i] <<std::endl;
      return {};
    }
  }
  return state;
}

void CountSolutions(State &state) {
#if 1
  CountResult cr = state.CountSolutions(max_count);
  assert(!cr.WorkLimitReached());
  if (cr.CountLimitReached()) std::cout << "At least ";
  std::cout << cr.count << " solutions" << std::endl;
  std::cout << "Work required: " << cr.work << std::endl;
  if (cr.count > 0) {
    std::cout << "Work required for first solution: "
        << state.CountSolutions(1).work << std::endl;
  }
#else
  // Slightly slower implementation using EnumerateSolutions() instead.
  int count = 0;
  EnumerateResult er = state.EnumerateSolutions([&count](const std::array<uint8_t, 81>&){
    return ++count < max_count;
  });
  if (!er.Accurate()) std::cout << "At least ";
  std::cout << count << " solutions" << std::endl;
#endif
}

bool Determined(unsigned mask) { return (mask & (mask - 1)) == 0; }

int GetSingleDigit(unsigned mask) {
  int d = 9;
  while (d > 0 && mask != (1u << d)) --d;
  return d;
}

// For each cell, calculates a bitmask of possible digits.
candidates_t CalculateOptions(std::span<const solution_t> solutions) {
  candidates_t options = {};
  for (const auto &solution : solutions) {
    for (int i = 0; i < 81; ++i) options[i] |= 1u << solution[i];
  }
  return options;
}

void EnumerateSolutions(State &state) {
  grid_t givens = {};
  for (int i = 0; i < 81; ++i) givens[i] = state.Digit(i);

  std::vector<solution_t> solutions;
  EnumerateResult er = state.EnumerateSolutions(solutions, max_count);

  // Print solutions (TODO: make this optional?)
  for (auto solution : solutions) {
    for (auto d : solution) std::cout << Char(d);
    std::cout << '\n';
  }

  assert(!er.WorkLimitReached());
  if (!er.success) {
    std::cout << "(further solutions omitted)\n";
    return;  // doesn't make sense to analyze options with incomplete data
  }

  int given_count = 0;
  for (int i = 0; i < 81; ++i) {
    std::cout << Char(givens[i]);
    if (givens[i]) ++given_count;
  }
  std::cout << " (" << given_count << " given)\n";

  candidates_t options = CalculateOptions(solutions);
  int inferred_count = 0;
  for (int i = 0; i < 81; ++i) {
    if (givens[i] != 0) {
      std::cout << '_';
    } else {
      int d = GetSingleDigit(options[i]);
      std::cout << Char(d);
      if (d) ++inferred_count;
    }
  }
  std::cout << " (" << inferred_count << " inferred)\n\n";

  int total_choices = 0;
  for (int d = 1; d <= 9; ++d) {
    int cells = 0;
    for (int i = 0; i < 81; ++i) {
      if (Determined(options[i])) {
        std::cout << '_';
      } else if (options[i] & (1u << d)) {
        std::cout << Char(d);
        ++cells;
      } else {
        std::cout << '.';
      }
    }
    std::cout << " (" << cells << " options)\n";
    total_choices += cells;
  }
  std::cout << '\n';
  for (int i = 0; i < 81; ++i) {
    if (Determined(options[i])) {
      std::cout << '_';
    } else {
      std::cout << std::popcount(options[i]);
    }
  }
  std::cout << " (choices per cell)\n";
  std::cout << total_choices << " (total choices)\n\n";

  // Print options also in a grid
  for (int r = 0; r < 9; ++r) {
    if (r > 0 && r % 3 == 0) std::cout << '\n';
    for (int y = 0; y < 3; ++y) {
      if (y == 1) std::cout << " " << (char) ('A' + r) << "  ";
      if (y != 1) std::cout << "    ";
      for (int c = 0; c < 9; ++c) {
        if (c > 0) {
          std::cout << "  ";
          if (c % 3 == 0) std::cout << "  ";
        }
        int i = 9*r + c;
        if (givens[i]) {
          // Given
          if (y == 0) std::cout << "===";
          if (y == 1) std::cout << " " << Char(givens[i]) << " ";
          if (y == 2) std::cout << "===";
        } else {
          unsigned o = options[i];
          int d = GetSingleDigit(o);
          if (d) {
            // Uniquely determined
            if (y == 0) std::cout << "---";
            if (y == 1) std::cout << " " << Char(d) << " ";
            if (y == 2) std::cout << "---";
          } else {
            // Options
            for (int x = 0; x < 3; ++x) {
              int d = 3*y + x + 1;
              std::cout << Char((o & (1u << d)) ? d : 0);
            }
          }
        }
      }
      std::cout << '\n';
    }
    std::cout << '\n';
  }
  std::cout << " ";
  for (int c = 0; c < 9; ++c) {
    if (c > 0 && c % 3 == 0) std::cout << "  ";
    std::cout << "    " << (char) ('a' + c);
  }
  std::cout << "\n\n" << std::flush;

  if (solutions.size() == 0) {
    std::cout << "No solution possible!\n";
  } else if (solutions.size() == 1) {
    std::cout << "Solution is unique!\n";
  } else {
    AnalyzeResult result = Analyze(givens, solutions);
    std::cout
        << "Result: " << result.outcome << " " << result.move
        << " (" << (char)('A' + result.move.pos/9) << (char)('a' + result.move.pos%9)
        << result.move.digit << (result.outcome == Outcome::WIN1 ? "!" : "") << ")\n\n"
        << counters << '\n';
  }
}

void Process(State &state) {
  CountSolutions(state);

  // TODO: make printing this optional
  EnumerateSolutions(state);
}

}  // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage:\n"
        "\tsolver <state>  (solves a single state)\n"
        "\tsolver -        (solve states read from standard input)\n";
    return 1;
  }
  if (strcmp(argv[1], "-") != 0) {
    // Process the state description passed as a command line argument.
    auto state = ParseDesc(argv[1]);
    if (!state) {
      std::cerr << "Could parse command line argument: [" << argv[1] << "]\n";
      return 1;
    }
    Process(*state);
  } else {
    // Process each line read from standard input.
    std::string line;
    for (int line_no = 1; std::getline(std::cin, line); ++line_no) {
      auto state = ParseDesc(line.c_str());
      if (!state) {
        std::cerr << "Parse error on line " << line_no << ": [" << line << "]\n";
        return 1;
      }
      Process(*state);
    }
  }
}
