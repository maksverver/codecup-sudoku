#!/bin/sh
#
# ./combine-sources.sh <source files>
#
# Concatenates C++ header and source files together, removing local includes,
# and preserving line number information, which is useful to make sense of
# compiler errors and debug log messages.

set -e -E -o pipefail

if [ $# = 0 ]; then
  echo "Usage: $0 <filenames>..."
  exit 1
fi

# Determine latest git commit (if possible) so we can include it in the build.
if git_commit=$(git rev-parse --short --verify HEAD) && [ -n "$git_commit" ]; then
  if ! git diff --quiet HEAD; then
    echo "WARNING: Git working directory has uncommitted changes!" >&2
    echo '#define GIT_DIRTY 1'
  fi
  echo "#define GIT_COMMIT \"${git_commit}\""
else
  echo 'WARNING: git rev-parse failed! Omitting git commit from output.' >&2
fi

for src in $@; do
  echo "#line 1 \"$src\""

  # Comment out #include "bla.h", but don't delete, to preserve line numbers.
  sed -e '/#include\s*"/s,^,//,' "$src"
done
