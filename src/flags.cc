#include "flags.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace {

struct FlagImpl {
  std::string help;
  std::string default_value;
  std::function<bool(std::string_view)> parse;
};

struct FlagMap {
  std::vector<std::string> ids;  // in registration order
  std::map<std::string, FlagImpl> by_id;
};

// This is a getter instead of a static variable to avoid problems related to
// initialization order, since RegisterFlag is typically called from a global
// initializer.
FlagMap &GetFlagMap() {
  static FlagMap map;
  return map;
}

bool ParseFlag(std::string_view s, const FlagMap &flag_map) {
  auto sep_pos = s.find('=');
  if (sep_pos == std::string_view::npos) sep_pos = s.size();
  if (s[0] != '-' || s[1] != '-' || sep_pos < 4) {
    std::cerr << "Incorrectly formatted command line argument: " << s << std::endl;
    return false;
  }
  std::string id(s.begin() + 2, s.begin() + sep_pos);
  auto it = flag_map.by_id.find(id);
  if (it == flag_map.by_id.end()) {
    std::cerr << "Unknown flag: " << id << std::endl;
    return false;
  }
  std::string_view arg;
  if (sep_pos < s.size()) arg = s.substr(sep_pos + 1);
  if (!it->second.parse(arg)) {
    std::cerr << "Failed to parse command line argument: " << s << std::endl;
    return false;
  }
  return true;
}

}  // namespace

void RegisterFlag(
    std::string_view id,
    std::string help,
    std::string default_value,
    std::function<bool(std::string_view)> parse) {
  assert(!id.empty());
  FlagMap &flag_map = GetFlagMap();
  if (!flag_map.by_id.insert({
        std::string(id),
        FlagImpl{
          .help=std::move(help),
          .default_value=std::move(default_value),
          .parse=std::move(parse)}
      }).second) {
    std::cerr << "Duplicate definition of flag " << id << std::endl;
    abort();
  }
  flag_map.ids.push_back(std::string(id));
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
  const FlagMap &flag_map = GetFlagMap();
  for (int i = 1; i < argc; ++i) {
    char *s = argv[i];
    if (s[0] == '-' && s[1] == '-' && s[2] == '\0') {
      // End-of-arguments marker ("--")
      while (i + 1 < argc) plain_args.push_back(argv[++i]);
      break;
    } else if (s[0] == '-' && s[1] != '\0') {
      // This should be a flag. Try to parse it.
      if (!ParseFlag(s, flag_map)) return false;
    } else {
      // Plain argument. Either it doesn't start with '-', OR it is the
      // single-character string "-" which often denoted stdin.
      plain_args.push_back(s);
    }
  }
  return true;
}

void PrintFlagUsage(std::ostream &os, std::string_view line_prefix) {
  FlagMap &flag_map = GetFlagMap();
  for (const std::string &id : flag_map.ids) {
    const FlagImpl &impl = flag_map.by_id.at(id);
    os << line_prefix << "--" << id;
    if (!impl.default_value.empty()) os << "=" << impl.default_value;
    if (!impl.help.empty()) os << ": " << impl.help;
    os << '\n';
  }
}
