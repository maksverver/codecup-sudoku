#include "state.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: solver <state>" << std::endl;
    return 1;
  }
  State state;
  const char *desc = argv[1];
  for (int i = 0; i < 81; ++i) {
    if (desc[i] >= '1' && desc[i] <= '9') {
      Move move = {i, desc[i] - '0'};
      if (!state.CanPlay(move)) {
        std::cerr << "Invalid digit as index " << i << std::endl;
        return 1;
      }
      state.Play(move);
    } else if (desc[i] != '.' && desc[i] != '0') {
      std::cerr << "Invalid character at index " << i << std::endl;
      return 1;
    }
  }
  std::cout << state.CountSolutions(1e18) << " solutions" << std::endl;
}
