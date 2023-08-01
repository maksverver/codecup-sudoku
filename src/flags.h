// Support for defining and parsing command line flags.

#ifndef FLAGS_H_INCLUDED
#define FLAGS_H_INCLUDED

#include <functional>
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

template<class T> bool ParseValue(std::string_view s, T &value) {
  std::istringstream iss((std::string(s)));
  return (iss >> value) && iss.peek() == std::istringstream::traits_type::eof();
}

template<> inline bool ParseValue<std::string>(std::string_view s, std::string &value) {
  value = s;
  return true;
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
