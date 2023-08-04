#!/usr/bin/env python3
#
# A very simple player that plays a uniformly random move, except that it never
# plays a move that makes the grid invalid. Used for testing.

from sudoku import CountSolutions
from random import shuffle
import sys

if __name__ == '__main__':
  line = sys.stdin.readline().strip()
  grid = [0]*81
  my_player = 0 if line == 'Start' else 1
  for turn in range(81):
    if turn % 2 == my_player:
      # My turn. Consider all possible moves.
      moves = [(i, d) for i, v in enumerate(grid) if v == 0 for d in range(1, 10)]
      shuffle(moves)
      for i, d in moves:
        grid[i] = d
        if CountSolutions(grid, 1):
          sys.stdout.write(chr(i // 9 + ord('A')) + chr(i % 9 + ord('a')) + chr(ord('0') + d) + '\n')
          sys.stdout.flush()
          break
        grid[i] = 0
      else:
        assert False  # there should be at least one valid move
    else:
      # Opponent's turn.
      if turn > 0:
        line = sys.stdin.readline().strip()
        if line == 'Quit':
          sys.exit(0)
      assert len(line) == 3
      i = (ord(line[0]) - ord('A'))*9 + (ord(line[1]) - ord('a'))
      assert grid[i] == 0
      d = ord(line[2]) - ord('0')
      assert 1 <= d <= 9
      grid[i] = d
