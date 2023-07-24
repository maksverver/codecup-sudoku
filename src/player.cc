#include "analysis.h"
#include "check.h"
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
#include <string>
#include <vector>

namespace {

const std::string player_name = "Numberwang!";

constexpr int max_work1 =   1000;
constexpr int max_work2 = 100000;
constexpr int max_count =   2000;

struct Timer {
  std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

  int64_t ElapsedMillis(bool reset = false) {
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int64_t result = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (reset) start = end;
    return result;
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

// TODO: remove this
#if 0
// Looks ahead only one move, picking a unique solution if possible.
std::optional<std::pair<Move, bool>> SelectMove(State &state) {
  std::vector<Move> moves[3];
  for (int i = 0; i < 81; ++i) if (state.IsFree(i)) {
    unsigned used = state.CellUsed(i);
    for (int d = 1; d <= 9; ++d) if ((used & (1u << d)) == 0) {
      Move move = {i, d};
      assert(state.CanPlay(move));
      state.Play(move);
      CountResult cr = state.CountSolutions(2, max_work1);
      state.Undo(move);
      if (cr.WorkLimitReached()) {
        std::cerr << ("Work limit exceeded! " + state.DebugString() + "\n");
      } else if (cr.count == 1) {
        // Actual winning move detected! Prioritize this over everything else.
        return {{move, true}};
      }
      moves[cr.count].push_back(move);
    }
  }
  // Pick a random move that keeps the most options open (accounting for the
  // fact that the move count may only be a lower bound if we ran out of time).
  int n = 2;
  while (n >= 0 && moves[n].empty()) --n;
  if (n < 0) return {};
  return {{RandomSample(moves[n]), false}};
}
#endif

std::string ReadInputLine() {
  std::string s;
  // I would rather do:
  //
  //  if (!std::getline(std::cin, s)) {
  //
  // but the CodeCup judging system sometimes writes empty lines before the
  // actual input! See: https://forum.codecup.nl/read.php?31,2221
  if (!(std::cin >> s)) {
    std::cerr << "Unexpected end of input!\n";
    exit(1);
  }
  if (s == "Quit") {
    std::cerr << "Quit received. Exiting.\n";
    exit(0);
  }
  std::cerr << ("Received: [" + s + "]\n");
  return s;
}

void WriteOutputLine(const std::string &s) {
  std::cerr << ("Sending: [" + s + "]\n");
  std::cout << s << std::endl;
}

Move PickRandomMove(const State &state) {
  std::vector<Move> moves;
  for (int pos = 0; pos < 81; ++pos) {
    if (state.Digit(pos) == 0) {
      int used = state.CellUsed(pos);
      for (int digit = 1; digit <= 9; ++digit) if ((used & (1u << digit)) == 0) {
        moves.push_back(Move{.pos = pos, .digit = digit});
      }
    }
  }
  return RandomSample(moves);
}

// Pick a move from an incomplete list of solutions.
// It returns a random move that maximizes the number of solutions remaining.
Move PickMoveIncomplete(const State &state, std::span<const solution_t> solutions) {
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
  return RandomSample(best_moves);
}

bool PlayGame() {
  std::string input = ReadInputLine();
  const int my_player = (input == "Start" ? 0 : 1);

  State state = {};
  std::vector<solution_t> solutions = {};
  bool analysis_complete = false;

  for (int turn = 0;; ++turn) {

    // Print current state for debugging.
    std::cerr << turn << ' ' << state.DebugString() << '\n';

    std::optional<Move> selected_move;

    if (turn % 2 == my_player) {
      // My turn!

      Timer timer;
      if (!analysis_complete) {
        EnumerateResult er = state.EnumerateSolutions(solutions, max_count, max_work2, &Rng());
        if (er.Accurate()) {
          analysis_complete = true;
          assert(!solutions.empty());
        } else if (solutions.empty()) {
          std::cerr << "WARNING: no solutions found! (this doesn't mean there aren't any)\n";
        }
      }

      std::cerr << solutions.size() << (analysis_complete ? "" : "+") <<
          " solutions in " << timer.ElapsedMillis(true) << " ms\n";

      bool claim_winning = false;

      if (solutions.empty()) {
        // I don't know anything about solutions. Just pick randomly.
        selected_move = PickRandomMove(state);
      } else if (!analysis_complete) {
        // I have some solutions but it's not the complete set.
        selected_move = PickMoveIncomplete(state, solutions);
      } else if (solutions.size() == 1) {
        // Only one solution left. I have already won! (This should be rare.)
        std::cerr << "Solution is already unique!\n";
        WriteOutputLine("!");
        return true;
      } else {
        // The hard case: select optimal move given the complete set of solutions.
        grid_t givens;
        for (int i = 0; i < 81; ++i) givens[i] = state.Digit(i);
        auto [move, winning] = SelectMoveFromSolutions(givens, solutions, nullptr);
        selected_move = move;
        claim_winning = winning;
        std::cerr << "Selected move in " << timer.ElapsedMillis(true) << " ms\n";

        // TODO: to save time, I should reuse the analysis information if both
        // players pick from the set of inferred digits!
      }

      // Output
      WriteOutputLine(FormatMove(*selected_move) + (claim_winning ? "!" :""));

    } else {
      // Opponent's turn.
      if (turn > 0) input = ReadInputLine();

      if (auto m = ParseMove(input); !m) {
        std::cerr << "Could not parse move!\n";
        return false;
      } else {
        selected_move = *m;
      }
    }

    assert(selected_move);
    if (!state.CanPlay(*selected_move)) {
      std::cerr << "Invalid move!\n";
      return false;
    }

    state.Play(*selected_move);

    if (!solutions.empty()) {
      if (!analysis_complete) {
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
        assert(!analysis_complete || !solutions.empty());
      }
    }
  }

  std::cerr << "Exiting normally.\n" << std::flush;
  return true;
}

} // namespace

int main() {
  std::cerr << player_name <<
      " (" <<std::numeric_limits<size_t>::digits << " bit)\n";

  return PlayGame() ? EXIT_SUCCESS : EXIT_FAILURE;
}
