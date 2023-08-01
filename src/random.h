#ifndef RANDOM_H_INCLUDED
#define RANDOM_H_INCLUDED

#include <cassert>
#include <optional>
#include <random>
#include <string>
#include <string_view>
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
namespace random::internal {

template<int> struct RngSelector {};
template<> struct RngSelector<32> { using rng_t = std::mt19937; };
template<> struct RngSelector<64> { using rng_t = std::mt19937_64; };

}  // namespace random::internal

using rng_t = internal::RngSelector<std::numeric_limits<size_t>::digits>::rng_t;

static_assert(sizeof(rng_t::result_type) == sizeof(size_t));

#endif

using rng_seed_t = std::vector<uint32_t>;

// Generates a random seed.
//
// Mersenne Twister's internal state is 19937 bits (slightly under 624 bytes).
// Initializing with a larger seed is not useful. Initializing with a smaller
// seed is usually fine!
rng_seed_t GenerateSeed(size_t size);

std::optional<rng_seed_t> ParseSeed(std::string_view hex_string);

std::string FormatSeed(const rng_seed_t &seed);

rng_t CreateRng(const rng_seed_t &seed);

template<class T> const T &RandomSample(const std::vector<T> &v, rng_t &rng) {
  assert(!v.empty());
  std::uniform_int_distribution<size_t> dist(0, v.size() - 1);
  return v[dist(rng)];
}

#endif // ndef RANDOM_H_INCLUDED
