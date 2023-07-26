#!/usr/bin/env pypy3

# This script contains some functions to explore a version of Nim, where in
# addition to the usual piles of tokens, we have a certain number of extra
# piles of one token each, which can be taken (one at a time) but that do not
# need to be emptied to win.
#
# That means a player wins when all the main piles are empty (regardless of
# wether any extra piles remain), but during their turn, they may choose to take
# exactly 1 token from an extra pile instead of moving in the main game.
#
# The point of the exercise, of course, is that the extra piles correspond with
# inferred digits in the Sudoku game grid, which can be filled in (one at a
# time) but does not narrow down the solution set.

from functools import reduce
from operator import xor

# GenPiles(*maximums) generates all lists L such that 0 <= L[i] < maximums[i].
# All maximums must be nonnegative integers. Each list is a new objec that may
# be modified by the recipient.
def GenPiles(*maximums):
  n = len(maximums)
  a = [0]*n
  while True:
    yield list(a)
    i = n - 1
    while i >= 0 and a[i] == maximums[i]:
      a[i] = 0
      i -= 1
    if i < 0:
      break
    a[i] += 1

assert list(GenPiles(1, 0, 2)) == [[0, 0, 0], [0, 0, 1], [0, 0, 2], [1, 0, 0], [1, 0, 1], [1, 0, 2]]


def Winning1(piles, extra):
  '''Solves the game given a list of pile sizes and the number of extra tokens.
     The list of piles is modified temporarly but restored before returning.'''
  if sum(piles) == 0:
    return False

  winning = False
  for i, v in enumerate(piles):
    for w in range(v):
      piles[i] = w
      if not Winning1(piles, extra):
        winning = True
      piles[i] = v
  if extra > 0 and not Winning1(piles, extra - 1):
    winning = True
  return winning


def Winning2(piles, extra):
  '''Same as Winning1() but returns as soon as a winning move is found.
     This is obviously (?) equivalent.'''
  if sum(piles) == 0:
    return False

  for i, v in enumerate(piles):
    for w in range(v):
      piles[i] = w
      winning = not Winning2(piles, extra)
      piles[i] = v
      if winning:
        return True
  if extra > 0 and not Winning2(piles, extra - 1):
    return True
  return False

winning_memo = {}

def Winning3(piles, extra):
  '''Same as Winning2(), but caches the results of previous computations in
     winning_memo, assuming the value of a position is determined exclusively
     by the sorted sequence of pile sizes and the extra number of tokens.
     i.e., order of piles doesn't matter, and neither do 0 elements.'''
  if sum(piles) == 0:
    return False

  key = tuple(sorted(x for x in piles if x > 0) + [extra])
  if key in winning_memo:
    return winning_memo[key]

  for i, v in enumerate(piles):
    for w in range(v):
      piles[i] = w
      winning = not Winning3(piles, extra)
      piles[i] = v
      if winning:
        winning_memo[key] = True
        return True
  if extra > 0 and not Winning3(piles, extra - 1):
    winning_memo[key] = True
    return True

  winning_memo[key] = False
  return False


def CheckWining1Equals2(maximums, extras, print_progress=False):
  for piles in GenPiles(*maximums):
    for extra in extras:
      if print_progress: print(piles, extra)
      copy1 = list(piles)
      copy2 = list(piles)
      if Winning1(copy1, extra) != Winning2(copy1, extra):
        return (piles, extra)  # counterexample
      assert piles == copy1 == copy2
  return None


def CheckWining2Equals3(maximums, extras, print_progress=False):
  for piles in GenPiles(*maximums):
    for extra in extras:
      if print_progress: print(piles, extra)
      copy1 = list(piles)
      copy2 = list(piles)
      if Winning2(piles, extra) != Winning3(piles, extra):
        return (piles, extra)  # counterexample
      assert piles == copy1 == copy2
  return None


def CheckSpragueGrundyTheorem(maximums, extras):
  '''The Sprague-Grundy theorem states that in regular Nim, multiple piles of
     tokens are equivalent to a single pile with the XOR sum of the sizes.

     Note that this may not hold when we have an "extra" pile that we do not
     need to exhaust!'''
  for piles in GenPiles(*maximums):
    for extra in extras:
      if Winning3(piles, extra) != Winning3([reduce(xor, piles)], extra):
        return (piles, extra)  # counterexample
  return None

def CheckOnlyParityOfExtrasMatters(maximums, extras):
  '''This checks that the winning/losing status does not depend on the exact
     value of `extras` but only on its parity (i.e., whether it is even or odd).'''
  for piles in GenPiles(*maximums):
    for extra in extras:
      if Winning3(piles, extra) != Winning3(piles, extra % 2):
        return (piles, extra)  # counterexample
  return None

def CheckOddExtrasInvertsResult(maximums, extras):
  '''This checks the hypothesis that the the parity of extras inverts the result
     of a regular Nim game. THIS DOES NOT HOLD! (See assertions below.)

     It does hold when extras is even, but not when it's odd.'''
  for piles in GenPiles(*maximums):
    if min(piles) > 0:
      for extra in extras:
        if Winning3(piles, extra) != (Winning3(piles, 0) ^ (extra % 2)):
          return (piles, extra)  # counterexample
  return None


assert CheckWining1Equals2([2, 4, 4], range(4)) is None
#assert CheckWining1Equals2([2, 6, 6], range(6), True) is None  # takes long
#assert CheckWining1Equals2([4, 4, 4], range(4), True) is None  # takes long

assert CheckWining2Equals3([5, 5, 5], range(5)) is None
#assert CheckWining2Equals3([6, 6, 6], range(6), True) is None  # takes long-ish

# Sprague Grundy theorem: piles is equal to nim of piles
assert CheckSpragueGrundyTheorem([20, 20, 20, 20], [0]) is None
assert CheckSpragueGrundyTheorem([10, 10, 10, 10, 10, 10], [0]) is None

# Sprague Grundy theorem also holds if extra is even.
#
# In that case we have a mirroring strategy: if the main game is winning and the
# opponent takes from the extra pile, then
assert CheckSpragueGrundyTheorem([10, 10, 10, 10], range(0, 11, 2)) is None
assert CheckSpragueGrundyTheorem([6, 6, 6, 6, 6, 6], range(0, 11, 2)) is None

assert CheckOnlyParityOfExtrasMatters([10]*4, range(11)) is None
assert CheckOnlyParityOfExtrasMatters([6]*6, range(11)) is None

# This is the important part: an odd number of extra tokens does not invert the
# result from the piles without an extra token!
#
# Note: in all of these states, both are winning (i.e., [1, 3], 1 and [1, 3], 0)
# are both winning. It's not possible for both to be losing, because if e.g.
# piles=[a, b c] extra=0 were losing, then piles=[a, b, c] extra=1 must be
# winning since we can move from the latter to the former.
assert CheckOddExtrasInvertsResult([10]*2, [1]) == ([1, 3], 1)
assert CheckOddExtrasInvertsResult([10]*2, [7]) == ([1, 3], 7)
assert CheckOddExtrasInvertsResult([10]*3, [1]) == ([1, 1, 2], 1)
assert CheckOddExtrasInvertsResult([10]*3, [9]) == ([1, 1, 2], 9)
assert CheckOddExtrasInvertsResult([10]*4, [1]) == ([1, 1, 1, 2], 1)
assert CheckOddExtrasInvertsResult([10]*4, [3]) == ([1, 1, 1, 2], 3)
assert CheckOddExtrasInvertsResult([10]*5, [1]) == ([1, 1, 1, 1, 2], 1)

# The reason inversion doesn't work is that there is a different optimal strategy
# in the presence of an extra pile.
#
# [1,3]+0 (win) -> [3]+0   (win immediately)
#               -> [1,2]+0 (win)    -> [2] (win) or (1, 1) loss
#               -> [1,1]+0 (loss)   -> [1] (win)
#
# [1,3]+1 (win) -> [3]+1    (win immediately, like before)
#                  [1,2]+1  (loss! inverted from previous) -> [1,2]+0 (win) or [2]+1 (win) or [1,1]+ (win)
#                  [1,1]+1  (win! inverted from previous) -> [1,1]+0 (loss) -> [1]+0 (win)
