#include "random.h"

#include <cassert>
#include <algorithm>
#include <random>

namespace {

// These hex parsing/formatting routines (see also ParseSeed() and FormatSeed()
// below) probably belong in a separate source file, but I'll move them when I
// need them somewhere else.

char FormatHexChar(uint32_t val) {
  return (val < 16) ? "0123456789abcdef"[val] : '?';
}

int ParseHexChar(char ch) {
  switch (ch) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
  }
  return -1;
}

}  // namespace;

rng_seed_t GenerateSeed(size_t size) {
  static_assert(sizeof(std::random_device::result_type) == sizeof(rng_seed_t::value_type));
  std::random_device dev;
  rng_seed_t result(size);
  std::generate_n(result.begin(), result.size(), std::ref(dev));
  return result;
}

std::optional<rng_seed_t> ParseSeed(std::string_view hex_string) {
  if (hex_string.size() % 8 != 0) return {};
  rng_seed_t seed(hex_string.size() / 8);
  size_t pos = 0;
  for (uint32_t &word : seed) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      int val = ParseHexChar(hex_string[pos++]);
      if (val < 0) return {};
      word |= val << shift;
    }
  }
  assert(pos == hex_string.size());
  return seed;
}

std::string FormatSeed(const rng_seed_t &seed) {
  std::string result(seed.size() * 8, '\0');
  size_t pos = 0;
  for (uint32_t word : seed) {
    for (int shift = 28; shift >= 0; shift -= 4) {
      result[pos++] = FormatHexChar((word >> shift) & 15);
    }
  }
  assert(pos == result.size());
  return result;
}

rng_t CreateRng(const rng_seed_t &seed) {
  std::seed_seq seq(seed.begin(), seed.end());
  return rng_t(seq);
}
