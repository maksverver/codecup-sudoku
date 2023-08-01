#!/usr/bin/env python3

# Arbiter for the 2024 CodeCup game Sudoku.

import argparse
from contextlib import nullcontext
import concurrent.futures
from collections import Counter, defaultdict
from datetime import datetime
from enum import Enum
import functools
import os.path
import subprocess
from subprocess import DEVNULL, PIPE
import shlex
import sudoku
import sys
import time


def ParseMove(s):
  if s == '!':
    return (None, True)

  w = s.endswith('!')
  if w:
    s = s[:-1]

  if len(s) == 3 and 'A' <= s[0] <= 'I' and 'a' <= s[1] <= 'i' and '1' <= s[2] <= '9':
    r = ord(s[0]) - ord('A')
    c = ord(s[1]) - ord('a')
    d = ord(s[2]) - ord('0')
    return ((r, c, d), w)

  return None


def FormatMove(move):
  r, c, d = move
  return chr(ord('A') + r) + chr(ord('a') + c) + chr(ord('0') + d)


class Outcome(Enum):
  WIN = 0           # regular win: either claimed win or opponent lost/crashed/etc
  LOSS = 1          # loss or tie
  UNSOLVABLE = 2    # move was valid but made grid unsolvable
  FAIL = 3          # crash, invalid move, etc.


def Launch(command, logfile):
  if logfile is None:
    stderr = DEVNULL
  else:
    stderr = open(logfile, 'wt')
  return subprocess.Popen(command, shell=True, text=True, stdin=PIPE, stdout=PIPE, stderr=stderr)


def RunGame(command1, command2, logfile1, logfile2, fast):
  roles = ['First', 'Second']
  commands = [command1, command2]
  procs = [Launch(command1, logfile1), Launch(command2, logfile2)]

  turn = 0
  grid = [0]*81
  solution_count = 2
  outcomes = [Outcome.LOSS, Outcome.LOSS]
  times = [0.0, 0.0]

  def Play(move):
    # Add digit to grid
    r, c, d = move
    i = 9*r + c
    assert grid[i] == 0
    grid[i] = d

    if not fast:
      # Recalculate solution count
      nonlocal solution_count
      solution_count = sudoku.CountSolutions(grid, 2)

  def Fail(outcome):
    outcomes[turn % 2] = outcome
    outcomes[1 - turn % 2] = Outcome.WIN

  for turn in range(81):
    proc = procs[turn % 2]

    # Send last move (or Start) to player
    if turn == 0:
      proc.stdin.write('Start\n')
    else:
      proc.stdin.write(FormatMove(move) + '\n')
    proc.stdin.flush()

    # Read player's move
    start_time = time.monotonic()
    line = proc.stdout.readline().strip()
    times[turn % 2] += time.monotonic() - start_time

    # Parse move
    parsed = ParseMove(line)
    if not parsed:
      Fail(Outcome.FAIL)
      break
    move, claim_win = parsed

    # Execute move
    if move is not None:
      Play(move)

      if solution_count == 0:
        # Player made
        Fail(Outcome.UNSOLVABLE)
        break

    # Player claimed the win.
    if claim_win:
      if fast:
        # Calculate solution count (since we haven't before in fast mode)
        solution_count = sudoku.CountSolutions(grid, 2)
      if solution_count == 1:
        outcomes[turn % 2] = Outcome.WIN
      else:
        Fail(Outcome.FAIL)
      break

  # Gracefully quit.
  for p in procs:
    if p.poll() is None:
      try:
        p.stdin.write('Quit\n')
        p.stdin.flush()
      except BrokenPipeError:
        pass

  for i, p in enumerate(procs):
    status = p.wait()
    if status != 0:
      print('%s command exited with nonzero status %d: %s' % (roles[i], status, commands[i]), file=sys.stderr)
      outcomes[i] = Outcome.FAIL

  return outcomes, times


def MakeLogfilenames(logdir, name1, name2, game_index, game_count):
  if not logdir:
    return (None, None)
  prefix = '%s-vs-%s' % (name1, name2)
  if game_count > 1:
    prefix = 'game-%s-of-%s-%s' % (game_index + 1, game_count, prefix)
  return (os.path.join(logdir, '%s-%s.txt' % (prefix, role))
      for role in ('first', 'second'))


class Tee:
  '''Writes output to a file, and also to stdout.'''

  def __init__(self, file):
    self.file = file

  def __enter__(self):
    return self

  def __exit__(self, exception_type, exception_value, traceback):
    self.file.close()

  def close(self, s):
    self.file.close()

  def write(self, s):
    sys.stdout.write(s)
    return self.file.write(s)


def RunGames(commands, names, rounds, logdir, fast=False, executor=None):
  P = len(commands)

  if rounds == 0:
    # Play only one match between ever pair of players, regardless of order
    pairings = [(i, j) for i in range(P) for j in range(i + 1, P)]
  else:
    # Play one match per round between every pair of players and each order
    pairings = [(i, j) for i in range(P) for j in range(P) if i != j] * rounds

  games_per_player = sum((i == 0) + (j == 0) for (i, j) in pairings)

  # Run all of the games, keeping track of total times and outcomes per player.
  player_time_total = [0.0]*P
  player_time_max   = [0.0]*P
  player_outcomes   = [defaultdict(lambda: 0) for _ in range(P)]

  def AddStatistics(i, j, outcomes, times):
    player_time_total[i] += times[0]
    player_time_total[j] += times[1]
    player_time_max[i] = max(player_time_max[i], times[0])
    player_time_max[j] = max(player_time_max[j], times[1])
    player_outcomes[i][outcomes[0]] += 1
    player_outcomes[j][outcomes[1]] += 1

  futures = []

  with (Tee(open(os.path.join(logdir, 'results.txt'), 'wt'))
        if logdir is not None and len(pairings) > 0 else nullcontext()) as f:

    print('Game Player 1           Player 2           Outcome 1  Outcome 2  Time 1 Time 2', file=f)
    print('---- ------------------ ------------------ ---------- ---------- ------ ------', file=f)

    def PrintRowStart(game_index, name1, name2):
      print('%4d %-18s %-18s ' % (game_index + 1, name1, name2), end='', file=f)
    def PrintRowFinish(outcomes, times):
      print('%-10s %-10s %6.2f %6.2f' % (outcomes[0].name, outcomes[1].name, times[0], times[1]), file=f)

    for game_index in range(len(pairings)):
      i, j = pairings[game_index]
      command1, command2 = commands[i], commands[j]
      name1, name2 = names[i], names[j]
      logfilename1, logfilename2 = MakeLogfilenames(logdir, name1, name2, game_index, len(pairings))

      run = functools.partial(RunGame, command1, command2, logfilename1, logfilename2, fast)

      if executor is None:
        # Run game on the main thread.
        # Print start of row first and flush, so the user can see which players
        # are competing while the game is in progress.
        PrintRowStart(game_index, name1, name2)
        sys.stdout.flush()
        outcomes, times = run()
        PrintRowFinish(outcomes, times)
        AddStatistics(i, j, outcomes, times)
      else:
        # Start the game on a background thread.
        future = executor.submit(run)
        futures.append(future)

    # Print results from asynchronous tasks in the order they were started.
    #
    # This means that earlier tasks can block printing of results for later tasks
    # have have already finished, but this is better than printing the results out
    # of order.
    for game_index, future in enumerate(futures):
      i, j = pairings[game_index]
      outcomes, times = future.result()
      AddStatistics(i, j, outcomes, times)
      PrintRowStart(game_index, names[i], names[j])
      PrintRowFinish(outcomes, times)

    print('---- ------------------ ------------------ ---------- ---------- ------ ------', file=f)

  # Print summary of players.
  if len(pairings) > 1:
    print()
    with (Tee(open(os.path.join(logdir, 'summary.txt'), 'wt')) if logdir is not None
          else nullcontext()) as f:
      print('Player             Avg.Tm Max.Tm Wins Loss Unsl Fail Tot.', file=f)
      print('------------------ ------ ------ ---- ---- ---- ---- ----', file=f)
      for p in sorted(range(P), key=lambda p: player_outcomes[p][Outcome.WIN], reverse=True):
        print('%-18s %6.2f %6.2f %4d %4d %4d %4d %4d' %
          ( names[p],
            player_time_total[p] / games_per_player,
            player_time_max[p],
            player_outcomes[p][Outcome.WIN],
            player_outcomes[p][Outcome.LOSS],
            player_outcomes[p][Outcome.UNSOLVABLE],
            player_outcomes[p][Outcome.FAIL],
            games_per_player,
          ), file=f)
      print('------------------ ------ ------ ---- ---- ---- ---- -----', file=f)


def DeduplicateNames(names):
  unique_names = []
  name_count = Counter(names)
  name_index = defaultdict(lambda: 0)
  for name in names:
    if name_count[name] == 1:
      unique_names.append(name)
    else:
      name_index[name] += 1
      unique_names.append('%s-%d' % (name, name_index[name]))
  return unique_names


def MakeLogdir(logdir, rounds):
  if logdir is None:
    return None

  assert os.path.isdir(logdir)
  if rounds > 0:
    # Create a subdirectory based on current date & time
    dirname = datetime.now().strftime('%Y%m%dT%H%M%S')
    logdir = os.path.join(logdir, dirname)
    os.mkdir(logdir)  # fails if path already exists
  return logdir


def Main():
  parser = argparse.ArgumentParser(prog='arbiter.py')
  parser.add_argument('command1', type=str,
      help='command to execute for player 1')
  parser.add_argument('command2', type=str,
      help='command to execute for player 2')
  parser.add_argument('commandN', type=str, nargs='*',
      help='command to execute for player N')
  parser.add_argument('--rounds', type=int, default=0,
      help='number of full rounds to play (or 0 for a single game)')
  parser.add_argument('--logdir', type=str, default=None,
      help='directory where to write player logs')
  parser.add_argument('-f', '--fast', action='store_true',
      help='increase speed by disabling checking for unsolvable grids')
  parser.add_argument('-t', '--threads', type=int, default=0,
      help='parallelize execution using threads (no more than physical cores!)')

  args = parser.parse_args()
  logdir = MakeLogdir(args.logdir, args.rounds)

  commands = [args.command1, args.command2] + args.commandN

  names = DeduplicateNames([os.path.basename(shlex.split(command)[0]) for command in commands])

  with (concurrent.futures.ThreadPoolExecutor(max_workers=args.threads)
        if args.threads > 0 else nullcontext()) as executor:
    RunGames(commands, names, rounds=args.rounds, logdir=logdir, fast=args.fast, executor=executor)

if __name__ == '__main__':
  Main()
