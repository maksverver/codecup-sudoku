// This file contains implementations of memos used during analysis.
//
// A memo is effectively a cache, where the key is an ordered-independent hash
// of the subset of solutions being considered, and the value is the result of
// IsWinning2(). This is used in analysis.cc to cache computations, which is
// beneficial because different sequences of moves often lead to the same
// solutions.

#ifndef MEMO_H_INCLUDED
#define MEMO_H_INCLUDED

#include "counters.h"

#include <array>
#include <cassert>
#include <cstdint>
#include <unordered_map>

using memo_key_t = uint64_t;

// Dummy memo that can be used to disable the memo.
class DummyMemo {
public:
  struct Value {
    bool HasValue() const { return false; }
    bool GetWinning() const { assert(false); }
    void SetWinning(bool b) { (void) b; }
  };

  Value Lookup(memo_key_t key) { (void) key; return Value(); }
};

// Write-only memo that can be used to measure performance overhead and verify
// correctness (i.e. for the same hash, the same result written).
class WriteonlyMemo {
public:
  struct Value {
    uint8_t *data = nullptr;

    bool HasValue() const { return false; }
    bool GetWinning() const { assert(false); }
    void SetWinning(bool b) {
      if (HasValue()) assert(GetWinning() == b);
      *data = b + 1;
    }
  };

  Value Lookup(memo_key_t key) { return Value{&data[key]}; }

private:
  std::unordered_map<memo_key_t, uint8_t> data;
};

// Real memo implementation based on std::unordered_map<>.
//
// Note: the simple implementation of Value::SetWinning() relies on the fact
// that entries in std::unordered_map<> are NOT invalidated by modifications,
// which is important because typically the memo is modified between the calls
// to HasValue() and SetWinning().
//
// This assumption doesn't hold for most flat hash table implementations!
class RealMemo {
public:
  struct Value {
    uint8_t *data = nullptr;

    bool HasValue() const { return *data != 0; }
    bool GetWinning() const { return *data - 1; }
    void SetWinning(bool b) { *data = b + 1; }
  };

  Value Lookup(memo_key_t key) { return Value{&data[key]}; }

private:
  std::unordered_map<memo_key_t, uint8_t> data;
};

// A memo based on a lossy hash table.
//
// The data is stored in an array of 64-bit integers. Each entry stores:
//
//  - The top 56 bits of the memo key. (Note that the lower bits are redundant
//    when size is a power of 2, since they are implied by the index in the
//    array, so we could get away with storing even less.)
//
//  - An 8 bit value describing the status of the position:
//    0 for uninitialized, 1 for losing, 2 for winning.
//
// If two hash keys map to the same array entry, whichever one called
// SetWinning() last wins.
class LossyMemo {
public:
  // 64 × 2^20 = about 67 million entries. Each entry takes 8 bytes, so total memory used is 512 MB.
  static const size_t size = 64 << 20;

  static_assert(size > 0 && (size & (size - 1)) == 0, "size must be a power of 2");

  static constexpr uint64_t value_mask = 0xff;
  static constexpr uint64_t key_mask = ~value_mask;

  struct Value {
    uint64_t masked_key;
    uint64_t *entry;

    bool HasValue() const {
      return (*entry & key_mask) == masked_key && (*entry & value_mask) != 0;
    }

    bool GetWinning() const {
      return (*entry & value_mask) - 1;
    }

    void SetWinning(bool b) {
      if ((*entry & key_mask) != 0 && (*entry & key_mask) != masked_key) [[unlikely]] {
        counters.memo_collisions.Inc();
      }

      // Unconditionally overwrite previous value!
      *entry = masked_key | (b + 1);
    }
  };

  Value Lookup(memo_key_t key) {
    return Value{key & key_mask, &data[(size_t) key % size]};
  }

private:
  std::array<uint64_t, size> data;
};

// Change the type of memo here to enable/disable memoization.
using memo_t = RealMemo;

#endif  // ndef MEMO_H_INCLUDED
