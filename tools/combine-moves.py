#!/usr/bin/env python3

# Creates or modifies a standard 81-character Sudoku grid description by adding
# or removing moves in CodeCup format to it.
#
# This tool takes a sole argument which is a string that starts either with a
# grid described with 81 characters, or a single move of the form Aa1, followed
# by a sequence of updates, where each update is of the form +Ab2 to add a digit
# to the grid or -Aa1 to remove an existing digit from the grid.
#
# Example 1:
#
# % tools/combine-moves.py Fb2+Ha3+Bb8+Fd6+Ig7
# ..........8...................................2.6..............3..............7..
#
#    a b c   d e f   g h i
#  A . . . | . . . | . . .
#  B . 8 . | . . . | . . .
#  C . . . | . . . | . . .
#    ------+-------+------
#  D . . . | . . . | . . .
#  E . . . | . . . | . . .
#  F . 2 . | 6 . . | . . .
#    ------+-------+------
#  G . . . | . . . | . . .
#  H 3 . . | . . . | . . .
#  I . . . | . . . | 7 . .
#
#
# Example 2:
#
# % tools/combine-moves.py \
# ..........8...................................2.6..............3..............7..-Ig7+Ih1
# ..........8...................................2.6..............3...............1.
#
#    a b c   d e f   g h i
#  A . . . | . . . | . . .
#  B . 8 . | . . . | . . .
#  C . . . | . . . | . . .
#    ------+-------+------
#  D . . . | . . . | . . .
#  E . . . | . . . | . . .
#  F . 2 . | 6 . . | . . .
#    ------+-------+------
#  G . . . | . . . | . . .
#  H 3 . . | . . . | . . .
#  I . . . | . . . | . 1 .
#

import re
import sys

DESCRIPTION_PATTERN = re.compile(r'([.0-9]{81}|[A-I][a-i][1-9])((?:[+-][A-I][a-i][1-9])*)')
UPDATE_PATTERN = re.compile(r'[+-][A-I][a-i][1-9]')

PRETTY_GRID_TEMPLATE = '''
  a b c   d e f   g h i
A . . . | . . . | . . .
B . . . | . . . | . . .
C . . . | . . . | . . .
  ------+-------+------
D . . . | . . . | . . .
E . . . | . . . | . . .
F . . . | . . . | . . .
  ------+-------+------
G . . . | . . . | . . .
H . . . | . . . | . . .
I . . . | . . . | . . .'''.replace('.', '%s')


def ParseMove(s):
  if len(s) == 3 and 'A' <= s[0] <= 'I' and 'a' <= s[1] <= 'i' and '1' <= s[2] <= '9':
    r = ord(s[0]) - ord('A')
    c = ord(s[1]) - ord('a')
    d = ord(s[2]) - ord('0')
    return (r, c, d)
  return None


def ParseDigit(ch, zero='.'):
  if ch == zero:
    return 0
  if ord('0') <= ord(ch) <= ord('9'):
    return ord(ch) - ord('0')
  return None


def FormatDigit(digit, zero='.'):
  if digit == 0: return zero
  return chr(digit + ord('0'))


def Process(description):
  m = DESCRIPTION_PATTERN.match(description)
  if not m:
    print('Invalid argument:', description)
    return False

  # Get starting configuration
  first, rest = m.groups()
  if len(first) == 81:
    grid = list(map(ParseDigit, first))
  else:
    r, c, d = ParseMove(first)
    grid = [0]*81
    grid[9*r + c] = d

  # Apply updates (add/remove digits)
  for update in UPDATE_PATTERN.findall(rest):
    r, c, d = ParseMove(update[1:])
    if update[0] == '+':
      # Add digit to grid
      if grid[9*r + c] != 0:
        print('Invalid update:', update)
        return False
      grid[9*r + c] = d
    else:
      # Remove digit from grid
      assert update[0] == '-'
      if grid[9*r + c] != d:
        print('Invalid update:', update)
        return False
      grid[9*r + c] = 0

  # Print canonical grid to stdout
  print(''.join(map(FormatDigit, grid)))

  # Pretty print grid to stderr
  print(PRETTY_GRID_TEMPLATE % tuple(map(FormatDigit, grid)), file=sys.stderr)

  return True


if __name__ == '__main__':
  if len(sys.argv) != 2:
    print('Usage: %s <description>' % (sys.argv[0],))
    sys.exit(1)

  _, description = sys.argv
  if not Process(description):
    sys.exit(1)
