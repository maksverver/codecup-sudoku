#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <cassert>
#include <random>
#include <vector>

#if 1

using rng_t = std::mt19937;

#else

// Select the RNG implementation based on whether we are running in 32-bit or
// 64-bit mode. std::mt19937_64 may be up to twice as fast as std::mt19937 on
// 64-bit architectures.
//
// This is currently disabled to keep things simple, since player performance
// isn't very dependent on the RNG speed.
template<int> struct RngSelector {};
template<> struct RngSelector<32> { using rng_t = std::mt19937; };
template<> struct RngSelector<64> { using rng_t = std::mt19937_64; };
using rng_t = RngSelector<std::numeric_limits<size_t>::digits>::rng_t;
static_assert(sizeof(rng_t::result_type) == sizeof(size_t));

#endif

rng_t InitializeRng();

inline rng_t &Rng() {
  static rng_t rng = InitializeRng();
  return rng;
}

template<class T> const T &RandomSample(const std::vector<T> &v, rng_t &rng) {
  assert(!v.empty());
  std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
  return v[dist(rng)];
}

template<class T> const T &RandomSample(const std::vector<T> &v) {
  return RandomSample(v, Rng());
}

#endif // ndef RANDOM_H_INCLUDED
