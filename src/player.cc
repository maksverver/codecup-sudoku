#include "check.h"
#include "random.h"
#include "state.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int max_work = 10000;

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

std::optional<std::pair<Move, bool>> SelectMove(State &state) {
  std::vector<Move> moves[3];

  for (int i = 0; i < 81; ++i) if (state.IsFree(i)) {
    unsigned used = state.CellUsed(i);
    for (int d = 1; d <= 9; ++d) if ((used & (1u << d)) == 0) {
      Move move = {i, d};
      assert(state.CanPlay(move));
      state.Play(move);
      CountResult cr = state.CountSolutions(2, max_work);
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

std::string ReadInputLine() {
  std::string s;
  // I would rather do:
  //
  //  if (!std::getline(std::cin, s)) {
  //
  // but the CodeCup judging system doesn't seem to like it!
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

} // namespace

int main() {
  std::string input = ReadInputLine();
  const int my_player = (input == "Start" ? 0 : 1);

  State state;
  for (int turn = 0;; ++turn) {
    std::cerr << turn << ' ' << state.DebugString() << '\n';

    if (turn % 2 == my_player) {
      // My turn!

      // First, check if the current state is already unique.
      // (Maybe this isn't necessary? I can still make a winning move.)
      if (CountResult cr = state.CountSolutions(2, max_work); cr.WorkLimitReached()) {
        std::cerr << "Work limit exceeded.\n";
      } else if (cr.count == 0) {
        std::cerr << "No solutions left!\n";
        return 1;
      } else if (cr.count == 1) {
        // Solution is already unique! Claim the win.
        WriteOutputLine("!");
        break;
      }

      // Select my move and print it.
      if (auto m = SelectMove(state); !m) {
        std::cerr << "No valid moves left!\n";
        return 1;
      } else {
        auto [move, winning] = *m;
        CHECK(state.CanPlay(move));
        state.Play(move);

        WriteOutputLine(FormatMove(move) + (winning ? "!" :""));
      }

    } else {
      // Opponent's turn.
      if (turn > 0) input = ReadInputLine();

      if (auto m = ParseMove(input); !m) {
        std::cerr << "Could not parse move!\n";
        return 1;
      } else if (!state.CanPlay(*m)) {
        std::cerr << "Invalid move!\n";
        return 1;
      } else {
        state.Play(*m);
      }
    }
  }

  std::cerr << "Exiting normally.\n" << std::flush;
  return 0;
}
