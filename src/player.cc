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

DECLARE_FLAG(std::string, arg_seed, "", "seed");
DECLARE_FLAG(int,         arg_enumerate_max_count,     100'000, "enumerate_max_count");
DECLARE_FLAG(int64_t,     arg_enumerate_max_work,   10'000'000, "enumerate_max_work");
DECLARE_FLAG(int,         arg_analyze_max_count,        10'000, "analyze_max_count");
DECLARE_FLAG(int64_t,     arg_analyze_max_work,    120'000'000, "analyze_max_work");

struct Timer {
  using clock_t = std::chrono::steady_clock;

  clock_t::time_point start = clock_t::now();
  clock_t::duration elapsed{0};

  bool Paused() const {
    return start == clock_t::time_point::min();
  }

  log_duration_t Elapsed() {
    if (!Paused()) {
      auto end = clock_t::now();
      elapsed += end - start;
      start = end;
    }
    return std::chrono::duration_cast<log_duration_t>(elapsed);
  }

  void Pause() {
    assert(!Paused());
    elapsed += clock_t::now() - start;
    start = clock_t::time_point::min();
  }

  void Resume() {
    assert(Paused());
    start = clock_t::now();
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

std::string FormatTurn(const Turn &turn) {
  std::ostringstream oss;
  oss << turn;
  return oss.str();
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
      unsigned unused = state.CellUnused(pos);
      for (int digit = 1; digit <= 9; ++digit) if (unused & (1u << digit)) {
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
  size_t count[81][10] = {};
  for (const auto &solution : solutions) {
    for (int i = 0; i < 81; ++i) {
      ++count[i][solution[i]];
    }
  }

  std::vector<Move> best_moves;
  size_t max_count = 0;
  for (int pos = 0; pos < 81; ++pos) {
    if (state.Digit(pos) == 0) {
      for (int digit = 1; digit <= 9; ++digit) {
        size_t c = count[pos][digit];
        assert(c <= solutions.size());
#if MUST_REDUCE
        if (c == solutions.size()) continue;  // Must reduce solution set size!
#endif
#if MAXIMIZE_SOLUTIONS_REMAINING
        if (c > max_count) {
          max_count = c;
          best_moves.clear();
        }
        if (max_count > 0 && c == max_count) {
          best_moves.push_back(Move{.pos = pos, .digit = digit});
        }
#else
        if (c > 0) {
          best_moves.push_back(Move{.pos = pos, .digit = digit});
        }
#endif
      }
    }
  }
  assert(max_count > 0 && !best_moves.empty());
  return RandomSample(best_moves, rng);
}

bool PlayGame(rng_t &rng) {
  std::string input = ReadInputLine();
  const int my_player = (input == "Start" ? 0 : 1);

  Timer total_timer;

  State state = {};
  std::vector<solution_t> solutions = {};
  bool solutions_complete = false;
  bool winning = false;
  size_t analyze_max_count = arg_analyze_max_count;

  // Updates the game state and refines the solutions set after playing the given move.
  auto PlayMove = [&state, &solutions, &solutions_complete](const Move &move) {
    state.Play(move);

    if (!solutions.empty()) {
      if (!solutions_complete) {
        // Just clear solutions. We'll regenerate them next turn.
        solutions.clear();
      } else {
        // Narrow down set of solutions.
        std::vector<solution_t> next_solutions;
        for (const auto &solution : solutions) {
          if (solution[move.pos] == move.digit) {
            next_solutions.push_back(solution);
          }
        }
        solutions.swap(next_solutions);
        assert(!solutions.empty());
      }
    }
  };

  for (int turn = 0;; ++turn) {
    if (turn % 2 == my_player) {
      // My turn!

      // Print current state for debugging. Ideally I would print this every
      // turn, but the log output is getting close to CodeCup's 10,000 character
      // limit, so I only do it on my own player's turn.
      LogTurn(turn, state, total_timer.Elapsed());

      Timer turn_timer;
      log_duration_t enumerate_time(0);
      log_duration_t analyze_time(0);
      if (!solutions_complete) {
        // Try to enumerate all solutions.
        Timer timer;
        EnumerateResult er = state.EnumerateSolutions(
            solutions, arg_enumerate_max_count, arg_enumerate_max_work, &rng);
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

      Turn turn;
      if (solutions.empty()) {
        // I don't know anything about solutions. Just pick randomly.
        turn = Turn(PickRandomMove(state, rng));
      } else if (!solutions_complete || solutions.size() > analyze_max_count) {
        // I have some solutions but it's not the complete set.
        turn = Turn(PickMoveIncomplete(state, solutions, rng));
      } else {
        // The hard case: select optimal move given the complete set of solutions.
        Timer timer;
        grid_t givens = {};
        for (int i = 0; i < 81; ++i) givens[i] = state.Digit(i);
        AnalyzeResult result = Analyze(givens, solutions, 1, arg_analyze_max_work);
        analyze_time += timer.Elapsed();
        if (!result.outcome) {
          LogWarning() << "Analysis aborted!";
          // Fall back to pseudo-random selection.
          turn = Turn(PickMoveIncomplete(state, solutions, rng));
          // Reduce max_analyze so that we don't try to re-analyze until the
          // solution set is smaller.
          analyze_max_count = solutions.size() - 1;
        } else {
          turn = RandomSample(result.optimal_turns, rng);
          LogOutcome(*result.outcome);
          if (turn.claim_unique) LogInfo() << "That's Numberwang!";
          // Detect bugs in analysis:
          bool new_winning = IsWinning(*result.outcome);
          if (winning && !new_winning) {
            LogWarning() << "State went from winning to losing! "
                << "(this means there is a bug in analysis)";
          }
          winning = new_winning;
        }
      }

      // Execute my selected move.
      assert(!turn.Empty());
      for (int i = 0; i < turn.move_count; ++i) {
        if (!state.CanPlay(turn.moves[i])) {
          LogError() << "Move " << i + 1 << " of " << turn.move_count << " is invalid!\n";
          return false;
        }
        PlayMove(turn.moves[i]);
      }
      LogTime(turn_timer.Elapsed(), enumerate_time, analyze_time);
      // Note: we should pause the timer just before writing the output line,
      // since the referee may suspend our process immediately after.
      total_timer.Pause();
      WriteOutputLine(FormatTurn(turn));
    } else {
      // Opponent's turn.
      if (turn > 0) {
        input = ReadInputLine();
        total_timer.Resume();
      }
      if (auto m = ParseMove(input); !m) {
        LogError() << "Could not parse move!";
        return false;
      } else if (!state.CanPlay(*m)) {
        LogError() << "Invalid move received!";
        return false;
      } else {
        PlayMove(*m);
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
