#include "check.h"

#include <iostream>
#include <cstdlib>

[[noreturn]] void CheckFail(const char *file, int line, const char *expr) {
  std::cerr << file  << ":" << line << ":" << ": CHECK(" << expr << ") failed!" << std::endl;
  abort();
}
