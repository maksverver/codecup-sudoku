#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <cassert>
#include <random>
#include <span>

std::mt19937 InitializeRng();

inline std::mt19937 &Rng() {
  static std::mt19937 rng = InitializeRng();
  return rng;
}

template<class T> const T &RandomSample(const std::vector<T> &v, std::mt19937 &rng) {
  assert(!v.empty());
  std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
  return v[dist(rng)];
}

template<class T> const T &RandomSample(const std::vector<T> &v) {
  return RandomSample(v, Rng());
}

#endif // ndef RANDOM_H_INCLUDED
