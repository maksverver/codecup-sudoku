// Support for defining and parsing command line flags.

#ifndef FLAGS_H_INCLUDED
#define FLAGS_H_INCLUDED

#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>

// Registers a flag (typically, this is called only indirectly via DECLARE_FLAG()).
void RegisterFlag(std::string_view id, std::function<bool(std::string_view)> parse);

// Parses flags from command line arguments of the form `--id=value`.
//
// If all arguments could be parsed, this returns true. Otherwise, it prints
// an error message to stderr and returns false.
//
// argv[0] is not parsed (it usually contains the program name)
bool ParseFlags(int argc, char *argv[]);

// Same as above, but allows non-flag arguments that are stored in plain_args.
bool ParseFlags(int argc, char *argv[], std::vector<char *> &plain_args);

// Declare a variable with the given type and default value, and register a
// command line flag with the given identifier. For example:
//
//   DECLARE_FLAG(int, foo, 42, bar)
//
// declares a variable `int foo = 42` that can be overridden with "--bar=123".
//
#define DECLARE_FLAG(type, var_id, default_value, flag_id) \
  type var_id = (RegisterFlag(flag_id, ::flags::internal::Parser(&var_id)), default_value)

namespace flags::internal {

template<class T> bool ParseGenericValue(std::string_view s, T &value) {
  std::istringstream iss((std::string(s)));
  return (iss >> value) && iss.peek() == std::istringstream::traits_type::eof();
}

template<class T> bool ParseIntegralValue(std::string_view s, T &value) {
  static_assert(std::is_integral_v<T>);
  // First try parsing as an integer.
  if (ParseGenericValue(s, value)) {
    return true;
  }
  // If that fails, try parsing as a double in exponentatial notation (1.23e45)
  // but only if the result is finite, integral and fits in the destintion type.
  long double ld = 0.0;
  if (ParseGenericValue(s, ld) && std::isfinite(ld) && std::trunc(ld) == ld &&
      ld >= std::numeric_limits<T>::min() && ld <= std::numeric_limits<T>::max()) {
    value = ld;
    return true;
  }
  return false;
}

// No implementation. Only parsing specific types is supported (see specializations below).
template<class T> bool ParseValue(std::string_view s, T &value);

template<> inline bool ParseValue<std::string>(std::string_view s, std::string &value) {
  value = s;
  return true;
}

template<> inline bool ParseValue<int>(std::string_view s, int &value) {
  return ParseIntegralValue(s, value);
}

template<> inline bool ParseValue<int64_t>(std::string_view s, int64_t &value) {
  return ParseIntegralValue(s, value);
}

template<typename T> class Parser {
public:
  Parser(T *value) : value(value) {}

  bool operator()(std::string_view s) {
    return ParseValue<T>(s, *value);
  }

private:
  T *value;
};

}  // namespace flags::internal

#endif
