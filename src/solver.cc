#include "state.h"

#include <iostream>
#include <cstring>

bool Solve(const char *desc) {
  State state;
  for (int i = 0; i < 81; ++i) {
    if (desc[i] >= '1' && desc[i] <= '9') {
      Move move = {i, desc[i] - '0'};
      if (!state.CanPlay(move)) {
        std::cerr << "Invalid digit as index " << i << std::endl;
        return false;
      }
      state.Play(move);
    } else if (desc[i] != '.' && desc[i] != '0') {
      std::cerr << "Invalid character at index " << i << std::endl;
      return false;
    }
  }
  CountResult cr = state.CountSolutions(1000);
  assert(!cr.WorkLimitReached());
  if (cr.CountLimitReached()) std::cout << "At least ";
  std::cout << cr.count << " solutions" << std::endl;
  return true;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage:\n"
        "\tsolver <state>  (solves a single state)\n"
        "\tsolver -        (solve states read from standard input)\n";
    return 1;
  }
  if (strcmp(argv[1], "-") == 0) {
    std::string line;
    while (std::getline(std::cin, line)) {
      if (!Solve(line.c_str())) return 1;
    }
  } else {
    if (!Solve(argv[1])) return 1;
  }
}
