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
  std::vector<Move> moves;
  for (int i = 0; i < 81; ++i) if (state.IsFree(i)) {
    unsigned used = state.CellUsed(i);
    for (int d = 1; d <= 9; ++d) if ((used & (1u << d)) == 0) {
      Move move = {i, d};
      assert(state.CanPlay(move));
      state.Play(move);
      int k = state.CountSolutions(max_work);
      state.Undo(move);
      if (k == 1) return {{move, true}};  // Winning move!
      if (k > 1) moves.push_back(move);
    }
  }
  if (moves.empty()) return {};
  return {{RandomSample(moves), false}};
}

std::string ReadInputLine() {
  std::string line;
  if (!std::getline(std::cin, line)) {
    std::cerr << "Unxpected end of input!" << std::endl;
    exit(1);
  }
  if (line == "Quit") {
    std::cerr << "Quit received. Exiting." << std::endl;
    exit(0);
  }
  return line;
}

} // namespace

int main() {
  std::string line = ReadInputLine();
  const int my_player = (line == "Start" ? 0 : 1);

  State state;
  for (int turn = 0;; ++turn) {
    std::cerr << turn << ' ' << state.DebugString() << '\n';

    if (turn % 2 == my_player) {
      // My turn! First, check if the current state is already unique.
      // (Maybe this isn't necessary?)
      if (int k = state.CountSolutions(max_work); k == 0) {
        std::cerr << "No solutions left!" << std::endl;
        return 1;
      } else if (k == 1) {
        std::cerr << "Sending: !" << std::endl;
        std::cout << "!" << std::endl;
        break;
      }

      // Select my move and print it.
      if (auto m = SelectMove(state); !m) {
        std::cerr << "No valid moves left!" << std::endl;
        return 1;
      } else {
        auto [move, winning] = *m;
        CHECK(state.CanPlay(move));
        state.Play(move);

        std::string output = FormatMove(move);
        if (winning) output += '!';
        std::cerr << "Sending " << output << std::endl;
        std::cout << output << std::endl;
      }

    } else {
      if (turn > 0) line = ReadInputLine();

      // Opponent's turn. I have already read the input in `line`.
      if (auto m = ParseMove(line); !m) {
        std::cerr << "Could not parse move: " << line << std::endl;
        return 1;
      } else if (!state.CanPlay(*m)) {
        std::cerr << "Invalid move: " << line << std::endl;
        return 1;
      } else {
        state.Play(*m);
      }
    }
  }

  std::cerr << "Exiting normally." << std::endl;
  return 0;
}
