#include "flags.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>

namespace {

struct FlagImpl {
  std::function<bool(std::string_view)> parse;
};

using flag_map_t = std::map<std::string, FlagImpl>;

// This is a getter instead of a static variable to avoid problems related to
// initialization order, since RegisterFlag is typically called from a global
// initializer.
flag_map_t &FlagsById() {
  static flag_map_t map;
  return map;
}

bool ParseFlag(std::string_view s, const flag_map_t &flags_by_id) {
  auto sep_pos = s.find('=');
  if (sep_pos == std::string_view::npos || sep_pos < 4 || s[0] != '-' || s[1] != '-') {
    std::cerr << "Incorrectly formatted command line argument: " << s << std::endl;
    return false;
  }
  std::string id(s.begin() + 2, s.begin() + sep_pos);
  auto it = flags_by_id.find(id);
  if (it == flags_by_id.end()) {
    std::cerr << "Unknown flag: " << id << std::endl;
    return false;
  }
  if (!it->second.parse(s.substr(sep_pos + 1))) {
    std::cerr << "Failed to parse command line argument: " << s << std::endl;
    return false;
  }
  return true;
}

}  // namespace

void RegisterFlag(std::string_view id, std::function<bool(std::string_view)> parse) {
  assert(!id.empty());
  if (!FlagsById().insert({std::string(id), FlagImpl{.parse=std::move(parse)}}).second) {
    std::cerr << "Duplicate definition of flag " << id << std::endl;
    abort();
  }
}

bool ParseFlags(int argc, char *argv[]) {
  std::vector<char*> plain_args;
  if (!ParseFlags(argc, argv, plain_args)) return false;
  if (!plain_args.empty()) {
    std::cerr << "Unexpected arguments:";
    for (char *s : plain_args) std::cerr << ' ' << s;
    std::cerr << std::endl;
    return false;
  }
  return true;
}

bool ParseFlags(int argc, char *argv[], std::vector<char*> &plain_args) {
  const flag_map_t &flags_by_id = FlagsById();
  for (int i = 1; i < argc; ++i) {
    char *s = argv[i];
    if (s[0] == '-' && s[1] == '-' && s[2] == '\0') {
      // End-of-arguments marker ("--")
      while (i + 1 < argc) plain_args.push_back(argv[++i]);
      break;
    } else if (s[0] == '-' && s[1] != '\0') {
      // This should be a flag. Try to parse it.
      if (!ParseFlag(s, flags_by_id)) return false;
    } else {
      // Plain argument. Either it doesn't start with '-', OR it is the
      // single-character string "-" which often denoted stdin.
      plain_args.push_back(s);
    }
  }
  return true;
}
