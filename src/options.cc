#include "options.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

namespace {

struct OptionImpl {
  std::string help;
  std::string default_value;
  std::function<bool(std::string_view)> parse;
};

struct OptionMap {
  std::vector<std::string> ids;  // in registration order
  std::map<std::string, OptionImpl> by_id;
};

// This is a getter instead of a static variable to avoid problems related to
// initialization order, since RegisterOption is typically called from a global
// initializer.
OptionMap &GetOptionMap() {
  static OptionMap map;
  return map;
}

bool ParseOption(std::string_view s, const OptionMap &option_map) {
  auto sep_pos = s.find('=');
  if (sep_pos == std::string_view::npos) sep_pos = s.size();
  if (s[0] != '-' || s[1] != '-' || sep_pos < 4) {
    std::cerr << "Incorrectly formatted command line argument: " << s << std::endl;
    return false;
  }
  std::string id(s.begin() + 2, s.begin() + sep_pos);
  auto it = option_map.by_id.find(id);
  if (it == option_map.by_id.end()) {
    std::cerr << "Unknown option: " << id << std::endl;
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

void RegisterOption(
    std::string_view id,
    std::string help,
    std::string default_value,
    std::function<bool(std::string_view)> parse) {
  assert(!id.empty());
  OptionMap &option_map = GetOptionMap();
  if (!option_map.by_id.insert({
        std::string(id),
        OptionImpl{
          .help=std::move(help),
          .default_value=std::move(default_value),
          .parse=std::move(parse)}
      }).second) {
    std::cerr << "Duplicate definition of option " << id << std::endl;
    abort();
  }
  option_map.ids.push_back(std::string(id));
}

bool ParseOptions(int argc, char *argv[]) {
  std::vector<char*> plain_args;
  if (!ParseOptions(argc, argv, plain_args)) return false;
  if (!plain_args.empty()) {
    std::cerr << "Unexpected arguments:";
    for (char *s : plain_args) std::cerr << ' ' << s;
    std::cerr << std::endl;
    return false;
  }
  return true;
}

bool ParseOptions(int argc, char *argv[], std::vector<char*> &plain_args) {
  const OptionMap &option_map = GetOptionMap();
  for (int i = 1; i < argc; ++i) {
    char *s = argv[i];
    if (s[0] == '-' && s[1] == '-' && s[2] == '\0') {
      // End-of-arguments marker ("--")
      while (i + 1 < argc) plain_args.push_back(argv[++i]);
      break;
    } else if (s[0] == '-' && s[1] != '\0') {
      // This should be a option. Try to parse it.
      if (!ParseOption(s, option_map)) return false;
    } else {
      // Plain argument. Either it doesn't start with '-', OR it is the
      // single-character string "-" which often denoted stdin.
      plain_args.push_back(s);
    }
  }
  return true;
}

void PrintOptionUsage(std::ostream &os, std::string_view line_prefix) {
  OptionMap &option_map = GetOptionMap();
  for (const std::string &id : option_map.ids) {
    const OptionImpl &impl = option_map.by_id.at(id);
    os << line_prefix << "--" << id;
    if (!impl.default_value.empty()) os << "=" << impl.default_value;
    if (!impl.help.empty()) os << ": " << impl.help;
    os << '\n';
  }
}
