// Support for defining and parsing command line options.

#ifndef OPTIONS_H_INCLUDED
#define OPTIONS_H_INCLUDED

#include <cmath>
#include <functional>
#include <limits>
#include <ostream>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>

// Registers an option (typically, this is called only indirectly via DECLARE_OPTION()).
void RegisterOption(
    std::string_view id,
    std::string help,
    std::string default_value,
    std::function<bool(std::string_view)> parse);

// Parses options from command line arguments of the form `--id=value`.
//
// If all arguments could be parsed, this returns true. Otherwise, it prints
// an error message to stderr and returns false.
//
// argv[0] is not parsed (it usually contains the program name)
bool ParseOptions(int argc, char *argv[]);

// Same as above, but allows non-option arguments that are stored in plain_args.
bool ParseOptions(int argc, char *argv[], std::vector<char *> &plain_args);

// Prints usage information of all registered options.
void PrintOptionUsage(std::ostream &os, std::string_view line_prefix="\t");

// Declare a variable with the given type and default value, and register a
// command line option with the given identifier. For example:
//
//   DECLARE_OPTION(int, foo, 42, "bar", "help text");
//
// declares a variable `int foo = 42` that can be overridden with "--bar=123".
// The "help text" may be an empty string but should not NULL. It is used by
// PrintOptionUsage(), which would describe the flag as:
//
//    --bar=42: help text
//
#define DECLARE_OPTION(type, var_id, default_value, option_id, option_help) \
  type var_id = ( \
      RegisterOption(option_id, option_help, \
          ::options::internal::FormatValue<type>(default_value), \
          ::options::internal::Parser<type>(&var_id)), \
      default_value)

namespace options::internal {

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

template<> inline bool ParseValue<bool>(std::string_view s, bool &value) {
  // Maybe later: support "on"/"off", and "yes"/"no".
  // Maybe later: support case insensitive-matching.
  if (s.empty() || s == "1" || s == "true") { value = true; return true; }
  if (s == "0" || s == "false") { value = false; return true; }
  return false;
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


template<class T> inline std::string FormatGenericValue(const T &value) {
  std::ostringstream oss;
  oss << value;
  return oss.str();
}

template<class T> std::string FormatValue(const T &value);

template<> inline std::string FormatValue<std::string>(const std::string &value) {
  return '"' + value + '"';
}

template<> inline std::string FormatValue<int>(const int &value) {
  return FormatGenericValue(value);
}

template<> inline std::string FormatValue<int64_t>(const int64_t &value) {
  return FormatGenericValue(value);
}

template<> inline std::string FormatValue<bool>(const bool &value) {
  return value ? "true" : "false";
}

}  // namespace options::internal

#endif  // ndef OPTIONS_H_INCLUDED
