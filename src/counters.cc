#include "counters.h"

Counters counters;

std::ostream &operator<<(std::ostream &os, const struct Counters &counters) {
  return os << "Counters{\n"
    << '\t' << counters.max_depth << ",\n"
    << "\t" << counters.recursive_calls << ",\n"
    << "\t" << counters.total_solutions << ",\n"
    << "\t" << counters.immediately_won << ",\n"
    << "\t" << counters.memo_accessed << ",\n"
    << "\t" << counters.memo_returned << ",\n"
    << "\t" << counters.memo_collisions << ",\n"
    << "}";
}
