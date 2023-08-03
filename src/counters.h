#ifndef COUNTERS_H_INCLUDED
#define COUNTERS_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>

template <typename T> class DummyCounter {
public:
  DummyCounter(const char *name);
  T Value() const { return 0; }
  void Inc() { }
  void Add(T unused) { (void) unused; }
  void SetMax(T unused) { (void) unused; }
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const DummyCounter<T> &) {
  return os;
}

template <typename T> struct RealCounter {
public:
  explicit RealCounter(const char *name) : name(name) {}
  const char *Name() const { return name; }
  T Value() const { return value; }
  void Inc() { ++value; }
  void Add(T v) { value += v; }
  void SetMax(T v) { if (v > value) value = v; }

private:
  const char *name;
  T value = 0;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const RealCounter<T> &counter) {
  return os << counter.Name() << '=' << counter.Value();
}

template <typename T> using counter_t = RealCounter<T>;

struct Counters {
  counter_t<int>     max_depth        = counter_t<int>("max_depth");
  counter_t<int64_t> recursive_calls  = counter_t<int64_t>("recursive_calls");
  counter_t<int64_t> total_solutions  = counter_t<int64_t>("total_solutions");
  counter_t<int64_t> immediately_won  = counter_t<int64_t>("immediately_won");
  counter_t<int64_t> memo_accessed    = counter_t<int64_t>("memo_accessed");
  counter_t<int64_t> memo_returned    = counter_t<int64_t>("memo_returned");
  counter_t<int64_t> memo_collisions  = counter_t<int64_t>("memo_collisions");
};

std::ostream &operator<<(std::ostream &os, const struct Counters &counters);

extern Counters counters;

#endif  // ndef COUNTERS_H_INCLUDED
