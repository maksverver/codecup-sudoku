#include "state.h"

#include <iostream>
#include <string>

std::string State::DebugString() const {
  std::string s(81, '.');
  for (int i = 0; i < 81; ++i) s[i] = digit[i] ? '0' + digit[i] : '.';
  return s;
}

void State::DebugPrint(std::ostream &os) const {
  for (int r = 0; r < 9; ++r) {
    for (int c = 0; c < 9; ++c) {
      if (c > 0) os << ' ';
      char ch = '0' + digit[9*r + c];
      os << (ch == '0' ? '.' : ch);
    }
    os << '\n';
  }
  os << '\n';
}
