# Overridable build variables
#
# Don't invoke this file directly. It is meant to be included in other files.

# Compiler flags
CXXFLAGS?=-std=c++20 -Wall -Wextra -pipe -DLOCAL_BUILD

# Linker flags
LDFLAGS?=
LDLIBS?=-lm

# Location of source files
SRC?=src/

# Location of temporary object files (dependent on compiler flags)
OBJ?=build/

# Location of final binaries (dependent on compiler flags)
BIN?=output/

# Generated output files (not dependent on compiler flags)
OUT?=output/
