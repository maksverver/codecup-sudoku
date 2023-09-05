#!/usr/bin/env python3
#
# Plays random moves, but always reduces solution set size.

from random import shuffle
import sudoku
import sys

# Set to true if each move must reduce the solution count.
MUST_REDUCE = True


def FormatGrid(grid):
  return ''.join('.' if d == 0 else str(d) for d in grid)


def FormatMove(r, c, d):
   return chr(ord('A') + r) + chr(ord('a') + c) + chr(ord('0') + d)


def Coords(pos):
  assert 0 <= pos < 81
  r = pos // 9
  c = pos % 9
  b = 3*(r // 3) + (c // 3)
  return (r, c, b)


class State:
  def __init__(self):
    self.grid = [0]*81
    all_digits = (1 << 10) - 2
    self.row_mask = 9*[all_digits]
    self.col_mask = 9*[all_digits]
    self.box_mask = 9*[all_digits]

  def Place(self, pos, digit):
    assert digit != 0 and self.grid[pos] == 0
    self.grid[pos] = digit
    r, c, b = Coords(pos)
    mask = 1 << digit
    self.row_mask[r] ^= mask
    self.col_mask[c] ^= mask
    self.box_mask[b] ^= mask

  def Unplace(self, pos, digit):
    assert digit != 0 and self.grid[pos] == digit
    self.grid[pos] = 0
    r, c, b = Coords(pos)
    mask = 1 << digit
    self.row_mask[r] ^= mask
    self.col_mask[c] ^= mask
    self.box_mask[b] ^= mask

  def Digit(self, pos):
    return self.grid[pos]

  def Fixed(self, pos):
    return self.grid[pos] != 0

  def CandidatesMask(self, pos):
    r, c, b = Coords(pos)
    return self.row_mask[r] & self.col_mask[c] & self.box_mask[b]

  def Candidates(self, pos):
    mask = self.CandidatesMask(pos)
    return [d for d in range(1, 10) if mask & (1 << d)]

  def CountSolutions(self, max_count):
    return sudoku.CountSolutions(self.grid, max_count)

  def HasSolution(self):
    return sudoku.HasSolution(self.grid)

  def IsSolvableMove(self, pos, digit):
    '''Returns whether the state would be sovable after performing move (pos, digit).'''
    self.Place(pos, digit)
    solvable = state.HasSolution()
    self.Unplace(pos, digit)
    return solvable

  def __str__(self):
    return FormatGrid(self.grid)


# Returns the first index i >= start such that moves[i] is a valid move that
# reduces the solution set size.
def PlayNextMove(state, moves, start):
  for i in range(start, len(moves)):
    pos, digit = moves[i]
    if state.Fixed(pos): continue

    candidates = state.Candidates(pos)
    if digit not in candidates: continue

    if not state.IsSolvableMove(pos, digit): continue

    if not MUST_REDUCE:
      # Valid move found.
      state.Place(pos, digit)
      return i

    else:
      # Check if there is another digit that could be placed here to give
      # a valid solution. If so, this move must reduce the solution count.
      other_solvable = False
      for other_digit in candidates:
        if other_digit != digit and state.IsSolvableMove(pos, other_digit):
          other_solvable = True
          break

      # Note: if other_solvable is False, we still place `digit` at `pos`,
      # because if can be determined uniquely, and doing so will speed up later
      # checks.
      state.Place(pos, digit)
      if other_solvable:
        return i

  return len(moves)


if __name__ == '__main__':
  if len(sys.argv) != 3:
    print('Usage: %s <num_grids> <max_solutions>' % (sys.argv[0],))
    sys.exit(1)

  num_grids, max_solutions = map(int, sys.argv[1:])
  for case in range(num_grids):
    state = State()
    moves = [(pos, digit) for pos in range(81) for digit in range(1, 10)]
    shuffle(moves)
    last_move_pos = -1

    # Play random moves until the solution is unique.
    grid = [0]*81
    history = []
    while (last_move_pos := PlayNextMove(state, moves, last_move_pos + 1)) < len(moves):
      pos, digit = moves[last_move_pos]
      grid[pos] = digit
      history.append(pos)
    assert sudoku.CountSolutions(grid, 2) == 1

    # Roll back history to last state with fewer than max_solutions solutions.
    solutions_count = None
    while True:
      pos = history.pop()
      digit = grid[pos]
      grid[pos] = 0
      last_solutions_count = solutions_count
      solutions_count = sudoku.CountSolutions(grid, max_solutions + 1)
      if solutions_count > max_solutions:
        # Redo last move
        solutions_count = last_solutions_count
        grid[pos] = digit
        history.append(pos)
        break

    assert sum(x != 0 for x in grid) == len(history)

    moves = [FormatMove(pos//9, pos%9, grid[pos]) for pos in history]

    print(case, len(history), solutions_count, FormatGrid(grid), ' '.join(moves), sep='\t')
