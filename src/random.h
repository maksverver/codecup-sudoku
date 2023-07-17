#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <cassert>
#include <random>
#include <vector>

extern std::mt19937 rng;

template<class T> const T &RandomSample(const std::vector<T> &v) {
  assert(!v.empty());
  std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
  return v[dist(rng)];
}

#endif // ndef RANDOM_H_INCLUDED
