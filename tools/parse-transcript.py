#!/usr/bin/env python3

import sys

def ParseMove(s):
  w = False
  if s.endswith('!'):
    w = True
    s = s[:-1]
  if len(s) == 3 and 'A' <= s[0] <= 'I' and 'a' <= s[1] <= 'i' and '1' <= s[2] <= '9':
    r = ord(s[0]) - ord('A')
    c = ord(s[1]) - ord('a')
    d = ord(s[2]) - ord('0')
    return (r, c, d, w)
  return None


def FormatDigit(digit, zero='.'):
  if digit == 0: return zero
  return chr(digit + ord('0'))


def FormatGrid(grid):
  return ''.join(map(FormatDigit, grid))


def ProcessTranscript(f):
  grid = [0]*81
  for line in f:
    s = line.strip()
    if s == '' or s.startswith('#'):
      continue
    r, c, d, w = ParseMove(s)
    assert grid[9*r + c] == 0
    grid[9*r + c] = d
    print('%-4s %s' % (s, FormatGrid(grid)))


def ParseDigit(ch, zero='.'):
  if ch == zero:
    return 0
  if ord('0') <= ord(ch) <= ord('9'):
    return ord(ch) - ord('0')
  return None


if __name__ == '__main__':
  if len(sys.argv) != 2:
    print('Usage: %s <filename>' % (sys.argv[0],))
    sys.exit(1)
  with open(sys.argv[1], 'rt') as f:
    ProcessTranscript(f)
