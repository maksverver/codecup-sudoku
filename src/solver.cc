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
  CountState cs = state.CountSolutions(1000);
  assert(!cs.WorkLimitExceeded());
  if (cs.CountLimitReached()) std::cout << "At least ";
  std::cout << cs.count << " solutions" << std::endl;
}
