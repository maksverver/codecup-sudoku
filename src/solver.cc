#include "analysis.h"
#include "counters.h"
#include "flags.h"
#include "state.h"

#include <array>
#include <bit>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

DECLARE_FLAG(bool, arg_help, false, "help", "");

DECLARE_FLAG(int64_t, analyze_max_work,        1e18, "analyze_max_work",
    "work limit for analysis");
DECLARE_FLAG(int64_t, analyze_batch_size,       1e7, "analyze_batch_size",
    "batch size for analsysis");
DECLARE_FLAG(int,     enumerate_max_count,      1e6, "enumerate_max_count",
    "max. number of solutions to enumerate");
DECLARE_FLAG(int,     max_print,                100, "max_print",
    "max. number of solutions to print");
DECLARE_FLAG(int,     max_winning_moves,          1, "max_winning_moves",
    "max. number of winning moves to list");

char Char(int d, char zero='.') {
  assert(d >= 0 && d < 10);
  return d == 0 ? zero : (char) ('0' + d);
}

// Parses a grid matching the regular expression: [0-9.]{81}
std::optional<State> ParseGridDesc(const char *desc) {
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
      std::cerr << "Invalid character at index " << i << ": " << desc[i] << std::endl;
      return {};
    }
  }
  return state;
}

// Parses a sequence of moves, optionally separated by non-alphanumeric characters.
std::optional<State> ParseMovesDesc(const char *desc) {
  State state;
  size_t i = 0, n = strlen(desc);
  while (i < n) {
    if (n - i < 3) {
      std::cerr << "Unexpected end of input at position " << i << std::endl;
      return {};
    }
    int r = desc[i++] - 'A';
    int c = desc[i++] - 'a';
    int d = desc[i++] - '0';
    if (!(r >= 0 && r < 9 && c >= 0 && c < 9 && d >= 1 && d <= 9)) {
      std::cerr << "Unparsable move at index " << i - 3 << std::endl;
      return {};
    }
    Move move = {.pos = 9*r + c, .digit = d};
    if (!state.CanPlay(move)) {
      std::cerr << "Invalid move at index " << i - 3 << ": " << move << std::endl;
      return {};
    }
    state.Play(move);
    if (i < n && !std::isalnum(desc[i])) ++i; // skip optional separator
  }
  return state;
}

std::optional<State> ParseDesc(const char *desc) {
  if (desc[0] >= 'A' && desc[0] <= 'I') return ParseMovesDesc(desc);
  return ParseGridDesc(desc);
}

void CountSolutions(State &state) {
#if 1
  CountResult cr = state.CountSolutions(enumerate_max_count);
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
    return ++count < enumerate_max_count;
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
  EnumerateResult er = state.EnumerateSolutions(solutions, enumerate_max_count);

  // Print solutions
  size_t print_count = std::min(solutions.size(), (size_t) max_print);
  for (size_t i = 0; i < print_count; ++i) {
    for (auto d : solutions[i]) std::cout << Char(d);
    std::cout << '\n';
  }

  assert(!er.WorkLimitReached());
  if (!er.success) {
    std::cout << "(further solutions omitted)\n";
    return;  // doesn't make sense to analyze options with incomplete data
  }
  if (print_count < solutions.size()) {
    std::cout << "(" << solutions.size() - print_count << " more solutions not printed)\n";
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
    AnalyzeResult result;
    int64_t work_left = analyze_max_work;
    for (;;) {
      int64_t max_work = std::min(work_left, analyze_batch_size);
      result = Analyze(givens, solutions, max_winning_moves, max_work);
      if (result.outcome) break;
      work_left -= max_work;
      if (work_left == 0) break;
      std::cout << "Analysis continuing..." << std::endl;
    }
    if (!result.outcome) {
      std::cout << "Analysis incomplete!" << std::endl;
    } else {
      std::cout << "Outcome: " << *result.outcome << '\n';
      std::cout << result.optimal_turns.size() << " optimal turns:";
      for (const Turn &turn : result.optimal_turns) std::cout << ' ' << turn;
      std::cout << '\n' << counters << '\n';
    }
  }
}

void Process(State &state) {
  CountSolutions(state);

  // TODO: make printing this optional
  EnumerateSolutions(state);
}

}  // namespace

int main(int argc, char *argv[]) {
  std::vector<char *> plain_args;
  if (!ParseFlags(argc, argv, plain_args) || plain_args.size() != 1 || arg_help) {
    std::clog << "Usage:\n"
        "\tsolver [<options>] <state>  (solves a single state)\n"
        "\tsolver [<options>] -        (solve states read from standard input)\n\n"
        "Options:\n";
    PrintFlagUsage(std::clog);
    return EXIT_FAILURE;
  }

  const char *arg = plain_args[0];
  if (strcmp(arg, "-") != 0) {
    // Process the state description passed as a command line argument.
    auto state = ParseDesc(arg);
    if (!state) {
      std::cerr << "Could parse command line argument: [" << arg << "]\n";
      return EXIT_FAILURE;
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
