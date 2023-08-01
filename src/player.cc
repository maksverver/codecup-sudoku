#include "analysis.h"
#include "check.h"
#include "flags.h"
#include "logging.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

const std::string player_name = "Numberwang";

DECLARE_FLAG(std::string, arg_seed,             "",         "seed");
DECLARE_FLAG(int64_t,     arg_max_work,         10'000'000, "max_work");
DECLARE_FLAG(int,         arg_max_enumerate,       100'000, "max_enumerate");
DECLARE_FLAG(int,         arg_max_analyze,           2'000, "max_analyze");

struct Timer {
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

  log_duration_t Elapsed(bool reset = false) {
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    auto elapsed = end - start;
    if (reset) start = end;
    return std::chrono::duration_cast<log_duration_t>(elapsed);
  }

  void Reset() {
    start = std::chrono::steady_clock::now();
  }
};

std::optional<Move> ParseMove(const std::string &s) {
  if (s.size() != 3 ||
      s[0] < 'A' || s[0] > 'I' ||
      s[1] < 'a' || s[1] > 'i' ||
      s[2] < '1' || s[2] > '9') return {};
  int row = s[0] - 'A';
  int col = s[1] - 'a';
  int digit = s[2] - '0';
  return Move{
    .pos = 9*row + col,
    .digit = digit};
}

std::string FormatMove(const Move &m) {
  m.AssertValid();
  std::string s(3, '\0');
  s[0] = 'A' + ((unsigned) m.pos / 9);
  s[1] = 'a' + ((unsigned) m.pos % 9);
  s[2] = '0' + m.digit;
  return s;
}

std::string ReadInputLine() {
  std::string s;
  // I would rather do:
  //
  //  if (!std::getline(std::cin, s)) {
  //
  // but the CodeCup judging system sometimes writes empty lines before the
  // actual input! See: https://forum.codecup.nl/read.php?31,2221
  if (!(std::cin >> s)) {
    LogError() << "Unexpected end of input!";
    exit(1);
  }
  LogReceived(s);
  if (s == "Quit") {
    LogInfo() << "Exiting.";
    exit(0);
  }
  return s;
}

void WriteOutputLine(const std::string &s) {
  LogSending(s);
  std::cout << s << std::endl;
}

Move PickRandomMove(const State &state, rng_t &rng) {
  std::vector<Move> moves;
  for (int pos = 0; pos < 81; ++pos) {
    if (state.Digit(pos) == 0) {
      int used = state.CellUsed(pos);
      for (int digit = 1; digit <= 9; ++digit) if ((used & (1u << digit)) == 0) {
        moves.push_back(Move{.pos = pos, .digit = digit});
      }
    }
  }
  return RandomSample(moves, rng);
}

// Pick a move from an incomplete list of solutions.
// It returns a random move that maximizes the number of solutions remaining.
Move PickMoveIncomplete(const State &state, std::span<const solution_t> solutions, rng_t &rng) {
  assert(!solutions.empty());
  int count[81][10] = {};
  for (const auto &solution : solutions) {
    for (int i = 0; i < 81; ++i) {
      ++count[i][solution[i]];
    }
  }

  std::vector<Move> best_moves;
  int max_count = 0;
  for (int pos = 0; pos < 81; ++pos) {
    if (state.Digit(pos) == 0) {
      for (int digit = 1; digit <= 9; ++digit) {
        int c = count[pos][digit];
        if (c > max_count) {
          max_count = c;
          best_moves.clear();
        }
        if (max_count > 0 && c == max_count) {
          best_moves.push_back(Move{.pos = pos, .digit = digit});
        }
      }
    }
  }
  assert(max_count > 0 && !best_moves.empty());
  return RandomSample(best_moves, rng);
}

bool PlayGame(rng_t &rng) {
  std::string input = ReadInputLine();
  const int my_player = (input == "Start" ? 0 : 1);

  log_duration_t total_time(0);
  Timer total_timer;

  State state = {};
  std::vector<solution_t> solutions = {};
  bool solutions_complete = false;
  bool winning = false;
  for (int turn = 0;; ++turn) {
    std::optional<Move> selected_move;
    if (turn % 2 == my_player) {

      // Print current state for debugging. Ideally I would print this every
      // turn for debugging, but the log output is getting close to the 10,000
      // character CodeCup limit.
      total_time += total_timer.Elapsed(true);
      LogTurn(turn, state, total_time);

      // My turn!
      log_duration_t enumerate_time(0);
      log_duration_t analyze_time(0);
      if (!solutions_complete) {
        // Try to enumerate all solutions.
        Timer timer;
        EnumerateResult er = state.EnumerateSolutions(solutions, arg_max_enumerate, arg_max_work, &rng);
        enumerate_time += timer.Elapsed();
        if (er.Accurate()) {
          solutions_complete = true;
          if (solutions.empty()) {
            LogError() << "No solutions remain!";
            return false;
          }
        } else if (solutions.empty()) {
          LogWarning() << "No solutions found! (this doesn't mean there aren't any)";
        }
      }
      LogSolutions(solutions.size(), solutions_complete);

      bool claim_winning = false;
      if (solutions.empty()) {
        // I don't know anything about solutions. Just pick randomly.
        selected_move = PickRandomMove(state, rng);
      } else if (!solutions_complete || solutions.size() > (size_t) arg_max_analyze) {
        // I have some solutions but it's not the complete set.
        selected_move = PickMoveIncomplete(state, solutions, rng);
      } else if (solutions.size() == 1) {
        // Only one solution left. I have already won! (This should be rare.)
        LogInfo() << "Solution is already unique!";
        WriteOutputLine("!");
        return true;
      } else {
        // The hard case: select optimal move given the complete set of solutions.
        Timer timer;
        grid_t givens = {};
        for (int i = 0; i < 81; ++i) givens[i] = state.Digit(i);
        AnalyzeResult result = Analyze(givens, solutions, &rng);
        analyze_time += timer.Elapsed();
        selected_move = result.move;
        claim_winning = result.outcome == Outcome::WIN1;
        LogOutcome(result.outcome);
        if (result.outcome == Outcome::WIN1) LogInfo() << "That's Numberwang!";
        // Detect bugs in analysis:
        bool new_winning = IsWinning(result.outcome);
        if (winning && !new_winning) {
          LogWarning() << "State went from winning to losing! "
              << "(this means there is a bug in analysis)";
        }
        winning = new_winning;
      }
      LogTime(total_timer.Elapsed(), enumerate_time, analyze_time);
      WriteOutputLine(FormatMove(*selected_move) + (claim_winning ? "!" :""));
    } else {
      // Opponent's turn.
      if (turn > 0) {
        total_time += total_timer.Elapsed();
        input = ReadInputLine();
        total_timer.Reset();
      }
      if (auto m = ParseMove(input); !m) {
        LogError() << "Could not parse move!";
        return false;
      } else {
        selected_move = *m;
      }
    }

    assert(selected_move);
    if (!state.CanPlay(*selected_move)) {
      LogError() << "Invalid move!";
      return false;
    }
    state.Play(*selected_move);

    if (!solutions.empty()) {
      if (!solutions_complete) {
        // Just clear solutions. We'll regenerate them next turn.
        solutions.clear();
      } else {
        // Narrow down set of solutions.
        std::vector<solution_t> next_solutions;
        for (const auto &solution : solutions) {
          if (solution[selected_move->pos] == selected_move->digit) {
            next_solutions.push_back(solution);
          }
        }
        solutions.swap(next_solutions);
        assert(!solutions_complete || !solutions.empty());
      }
    }
  }

  // This is probably unreachable?
  LogInfo() << "Game over.";
  return true;
}

bool InitializeSeed(rng_seed_t &seed, std::string_view hex_string) {
  if (hex_string.empty()) {
    // Generate a new random 128-bit seed
    seed = GenerateSeed(4);
    return true;
  }
  if (auto s = ParseSeed(hex_string)) {
    seed = *s;
    return true;
  } else {
    LogError() << "Could not parse RNG seed: [" << hex_string << "]";
    return false;
  }
}

} // namespace

int main(int argc, char *argv[]) {
  LogId(player_name);

  if (!ParseFlags(argc, argv)) return EXIT_FAILURE;

  // Initialize RNG.
  rng_seed_t seed;
  if (!InitializeSeed(seed, arg_seed)) return EXIT_FAILURE;
  LogSeed(seed);
  rng_t rng = CreateRng(seed);

  return PlayGame(rng) ? EXIT_SUCCESS : EXIT_FAILURE;
}
