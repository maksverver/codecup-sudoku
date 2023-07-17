#!/bin/sh
#
# ./combine-sources.sh <source files>
#
# Concatenates C++ header and source files together, removing local includes,
# and preserving line number information, which is useful to make sense of
# compiler errors and debug log messages.

set -e -E -o pipefail

for src in $@; do
  echo "#line 1 \"$src\""

  # Comment out #include "bla.h", but don't delete, to preserve line numbers.
  sed -e '/#include\s*"/s,^,//,' "$src"
done
