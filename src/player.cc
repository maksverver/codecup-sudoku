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

#ifndef LOCAL_BUILD
#define LOCAL_BUILD 0
#endif

namespace {

const std::string player_name = "Numberwang";

DECLARE_FLAG(bool, arg_help, false, "help", "");

DECLARE_FLAG(std::string, arg_seed, "", "seed",
    "Random seed in hexadecimal format. If empty, pick randomly. "
    "The chosen seed will be logged to stderr for reproducibility.");

DECLARE_FLAG(int, arg_enumerate_max_count, 200'000, "enumerate_max_count",
    "Maximum number of solutions to enumerate.");

DECLARE_FLAG(int64_t, arg_enumerate_max_work, 20'000'000, "enumerate_max_work",
    "Maximum number of recursive calls used to enumerate solutions.");

DECLARE_FLAG(int, arg_analyze_max_count, 100'000, "analyze_max_count",
    "Maximum number of solutions to enable analysis. That is, endgame analysis "
    "does not start until the solution count is less than or equal to this value.");

DECLARE_FLAG(int64_t, arg_analyze_max_work, 100'000'000, "analyze_max_work",
    "Maximum amount of work to perform during analysis (number of recursive calls "
    "times average number of solutions remaining). This only applies when no time "
    "limit is given.");

DECLARE_FLAG(int, arg_time_limit, LOCAL_BUILD ? 0 : 27, "time_limit",
    "Time limit in seconds (or 0 to disable time-based performance). "
    "On each turn, the player uses a fraction of time remaining on analysis. "
    "Note that this should be slightly lower than the official time limit to "
    "account for overhead.");

// Limit work done in a single call to Analyze(). This should be small enough
// to avoid timeouts, but large enough to make analysis efficient.
//
// Before the move ordering implemented in commit 331998f, 10 million
// corresponded with approximately 1 second on the CodeCup server, but this
// might not be true anymore!
DECLARE_FLAG(int64_t, arg_analyze_batch_size, 10'000'000, "analyze_batch_size",
    "Amount of work to do at once when using a time limit.");


// A simple timer. Can be running or paused. Tracks time both while running and
// while paused. Use Elapsed() to query, Pause() and Resume() to switch states.
class Timer {
public:
  Timer(bool running = true) : running(running) {}

  bool Running() const { return running; }
  bool Paused() const { return !running; }

  // Returns how much time passed in the given state, in total.
  log_duration_t Elapsed(bool while_running = true) {
    clock_t::duration d = elapsed[while_running];
    if (running == while_running) d += clock_t::now() - start;
    return std::chrono::duration_cast<log_duration_t>(d);
  }

  log_duration_t Pause() {
    assert(Running());
    return TogglePause();
  }

  log_duration_t Resume() {
    assert(Paused());
    return TogglePause();
  }

  // Toggles running state, and returns how much time passed since last toggle.
  log_duration_t TogglePause() {
    auto end = clock_t::now();
    auto delta = end - start;
    elapsed[running] += delta;
    start = end;
    running = !running;
    return std::chrono::duration_cast<log_duration_t>(delta);
  }

private:
  using clock_t = std::chrono::steady_clock;

  bool running = false;
  clock_t::time_point start = clock_t::now();
  clock_t::duration elapsed[2] = {clock_t::duration{0}, clock_t::duration{0}};
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
      // Skip cells that are known to be unique. (Passing this check doesn't
      // mean the move is valid, but it's not known to be invalid, which is
      // better than nothing!)
      if ((unused & (unused - 1)) == 0) continue;
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
#if MAXIMIZE_SOLUTIONS_REMAINING
  size_t max_count = 0;
#endif
  for (int pos = 0; pos < 81; ++pos) {
    if (state.Digit(pos) == 0) {
      for (int digit = 1; digit <= 9; ++digit) {
        size_t c = count[pos][digit];
        assert(c <= solutions.size());
        if (c == solutions.size()) continue;  // Must reduce solution set size!
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
#if MAXIMIZE_SOLUTIONS_REMAINING
  assert(max_count > 0);
#endif
  assert(!best_moves.empty());
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
        if (solutions.size() == next_solutions.size()) {
          LogWarning() << "Non-reducing move: " << move;
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
        AnalyzeResult result;
        if (arg_time_limit <= 0) {
          result = Analyze(givens, solutions, 1, arg_analyze_max_work);
        } else {
          // Heuristic: each turn, use 1/3 of the remaining time for analysis.
          // For a 30 second time limit this allocates: 10, 6.67, 4.44, 2.96, etc.
          // Maybe TODO: make the fraction a flag?
          log_duration_t
            time_elapsed = total_timer.Elapsed(),
            time_remaining = std::chrono::seconds(arg_time_limit) - time_elapsed,
            time_budget = time_remaining / 3;
          for (;;) {
            result = Analyze(givens, solutions, 1, arg_analyze_batch_size);
            if (result.outcome || timer.Elapsed() > time_budget) break;
            LogInfo() << "Continuing analysis";
          }
        }
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
        auto pause_duration = total_timer.Resume();
        LogPause(pause_duration, total_timer.Elapsed(false));
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

  if (!ParseFlags(argc, argv) || arg_help) {
    std::clog << "\nOptions:\n";
    PrintFlagUsage(std::clog);
    return EXIT_FAILURE;
  }

  // Initialize RNG.
  rng_seed_t seed;
  if (!InitializeSeed(seed, arg_seed)) return EXIT_FAILURE;
  LogSeed(seed);
  rng_t rng = CreateRng(seed);

  return PlayGame(rng) ? EXIT_SUCCESS : EXIT_FAILURE;
}
