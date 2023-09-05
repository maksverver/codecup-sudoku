#!/usr/bin/env python3

# Routines for solving Sudoku puzzles by reduction to SAT.

import pycosat
import sys


def GenerateSudokuCnf():
  '''Generates the constraints of a Sudoku puzzle in conjunctive normal form (CNF)'''

  def P(r, c, d):
    '''Predicate representing the fact that digit `d` is at row `r` and column `d`.'''
    assert 0 <= r < 9
    assert 0 <= c < 9
    assert 1 <= d <= 9
    return 9*9*r + 9*c + d

  def Q(b, i, d):
    '''Predicate representing the fact that digit `d` is in box `b` at index `d`.'''
    assert 0 <= b < 9
    assert 0 <= i < 9
    assert 1 <= d <= 9
    r = 3*(b // 3) + (i // 3)
    c = 3*(b % 3) + (i % 3)
    return 9*9*r + 9*c + d

  cnf = []

  # Each cell contains a value
  for r in range(9):
    for c in range(9):
      cnf.append([P(r, c, d) for d in range(1, 10)])

  # No row/column/box contains the same value twice
  for x in range(9):
    for i in range(9):
      for j in range(i + 1, 9):
        for d in range(1, 10):
          cnf.append([-P(x, i, d), -P(x, j, d)])
          cnf.append([-P(i, x, d), -P(j, x, d)])
          cnf.append([-Q(x, i, d), -Q(x, j, d)])

  # No cell contains more than one value (redundant, but helpful)
  for r in range(9):
    for c in range(9):
      for d in range(1, 10):
        for e in range(d + 1, 10):
          cnf.append([-P(r, c, d), -P(r, c, e)])

  return cnf


SUDOKU_CNF = tuple(GenerateSudokuCnf())

def MakeCnf(grid):
  cnf = list(SUDOKU_CNF)
  for i, v in enumerate(grid):
    if v: cnf.append([9*i + v])
  return cnf


def HasSolution(grid):
  cnf = MakeCnf(grid)
  res = pycosat.solve(cnf)
  if isinstance(res, list):
    return True
  assert res == 'UNSAT'
  return False


def CountSolutions(grid, max_count):
  cnf = MakeCnf(grid)
  count = 0
  for solution in pycosat.itersolve(cnf):
    count += 1
    if count >= max_count: break
  return count


def EnumerateSolutions(grid):
  cnf = MakeCnf(grid)
  for solution in pycosat.itersolve(cnf):
    solution_grid = [None]*81
    for v in solution:
      if v < 0: continue
      assert v > 0
      i = (v - 1) // 9
      d = (v - 1) % 9 + 1
      assert solution_grid[i] is None
      solution_grid[i] = d
    assert all(cell is not None for cell in solution_grid)
    yield solution_grid


def ParseDigit(ch, zero_char='.'):
  if ch == zero_char: return 0
  d = ord(ch) - ord('0')
  assert 0 <= d <= 9
  return d


def FormatDigit(d, zero_char='.'):
  if d == 0: return zero_char
  assert 1 <= d <= 9
  return chr(d + ord('0'))


if __name__ == '__main__':
  if len(sys.argv) != 2:
    print('Usage: sudoku.py <state>')
    sys.exit(1)

  grid = list(map(ParseDigit, sys.argv[1]))
  assert len(grid) == 81

  for solution in EnumerateSolutions(grid):
    print(''.join(map(FormatDigit, solution)))
