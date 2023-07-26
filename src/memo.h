// This file contains implementations of memos used during analysis.
//
// A memo is effectively a cache, where the key is an ordered-independent hash
// of the subset of solutions being considered, and the value is the result of
// IsWinning2(). This is used in analysis.cc to cache computations, which is
// beneficial because different sequences of moves often lead to the same
// solutions.

#ifndef MEMO_H_INCLUDED
#define MEMO_H_INCLUDED

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

// Real memo implementation.
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

class LossyMemo {
public:
  // 64 × 2^20 = about 67 million entries. Each entry takes 16 bytes, so total memory used is 1 GB.
  static const size_t size = 64 << 20;

  struct Entry {
    uint64_t key;
    uint8_t data;
  };

  static_assert(sizeof(Entry) == 16);

  struct Value {
    uint64_t key;
    Entry *entry;

    bool HasValue() const {
      return key == entry->key && entry->data != 0;
    }

    bool GetWinning() const {
      return entry->data - 1;
    }

    void SetWinning(bool b) {
      // TODO: stats on collisions?
      // if (entry->key != 0 && key != entry->key) ReportCollision();

      // Unconditionally overwrite previous value!
      entry->key = key;
      entry->data = b + 1;
    }
  };

  Value Lookup(memo_key_t key) {
    return Value{key, &data[(size_t) key % size]};
  }

private:
  std::array<Entry, size> data;
};

// Change the type of memo here to enable/disable memoization.
using memo_t = RealMemo;

#endif  // ndef MEMO_H_INCLUDED
