#ifndef COUNTERS_H_INCLUDED
#define COUNTERS_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <string>

template <typename T> class DummyCounter {
public:
  DummyCounter(const char *) {};
  T CurValue() const { return 0; }
  T MaxValue() const { return 0; }
  void Inc() {}
  void Add(T) {}
  void Dec() {}
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const DummyCounter<T> &) {
  return os;
}

template <typename T> struct RealCounter {
public:
  explicit RealCounter(const char *name) : name(name) {}
  const char *Name() const { return name; }
  T CurValue() const { return cur_value; }
  T MaxValue() const { return cur_value > max_value ? cur_value : max_value; }
  void Inc() { ++cur_value; }
  void Add(T v) { cur_value += v; }
  void Dec() {
    // Only update max_value when decrementing, to avoid performance penalty
    // for counters that only increase.
    if (cur_value > max_value) max_value = cur_value;
    --cur_value;
  }

private:
  const char *name;
  T cur_value = 0;
  T max_value = 0;
};

template <typename T>
std::ostream &operator<<(std::ostream &os, const RealCounter<T> &counter) {
  return os << counter.Name() << '=' << counter.MaxValue();
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
