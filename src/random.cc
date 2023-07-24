#include "random.h"

#include <random>

#ifdef DETERMINISTIC

rng_t InitializeRng() {
  return rng_t();
}

#else

#include <array>
#include <algorithm>

rng_t InitializeRng() {
  std::array<unsigned int, 624> seed_data;
  std::random_device dev;
  std::generate_n(seed_data.data(), seed_data.size(), std::ref(dev));
  std::seed_seq seq(std::begin(seed_data), std::end(seed_data));
  return rng_t(seq);
}

#endif
