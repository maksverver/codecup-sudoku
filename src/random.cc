#include "random.h"

#include <random>

#ifdef DETERMINISTIC

std::mt19937 rng;

#else

#include <array>
#include <algorithm>

static std::mt19937 InitializeRng() {
  std::array<int, 624> seed_data;
  std::random_device dev;
  std::generate_n(seed_data.data(), seed_data.size(), std::ref(dev));
  std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
  return std::mt19937(seq);
}

std::mt19937 rng = InitializeRng();

#endif
