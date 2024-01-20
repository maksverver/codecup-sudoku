#!/usr/bin/env python3

import csv
import json
import re
import os.path
import sys
import urllib.parse
import urllib.request

GAME_RE = re.compile(r'<script type="application/javascript">var g = new Sudoku\((\[.*?,.*?]), (\[.*?])\);</script>')
SHOWGAME_BASE_URL = 'https://www.codecup.nl/showgame.php'

class Game:
  def __init__(self, game_id, round, is_swiss):
    self.game_id  = game_id
    self.round    = round
    self.is_swiss = is_swiss
    self.user1    = None
    self.score1   = None
    self.status1  = None
    self.user2    = None
    self.score2   = None
    self.status2  = None
    self.moves    = None
    self.solution = None

  def __str__(self):
    return f'Game game_id={self.game_id} round={self.round} is_swiss={self.is_swiss} user1={self.user1} score1={self.score1} status1={self.status1} user2={self.user2} score2={self.score2} status2={self.status2}'


def ReadCompetitionCsv(filename):
  '''Returns a list of games in this competition.'''
  games_by_id = {}
  games = []
  with open(filename, 'rt', newline='') as f:
    for i, row in enumerate(csv.reader(f)):
      if i == 0:
        assert row == ['Game', 'Round', 'IsSwiss', 'Player', 'User', 'Score', 'Status']
      else:
        game_id, round, is_swiss, player, user, score, status = row
        if game_id not in games_by_id:
          game = Game(game_id, round, is_swiss)
          games_by_id[game_id] = game
          games.append(game)
        else:
          game = games_by_id[game_id]
        if player == 'First':
          assert game.user1 is None
          game.user1 = user
          game.score1 = score
          game.status1 = status
        elif player == 'Second':
          assert game.user2 is None
          game.user2 = user
          game.score2 = score
          game.status2 = status
        else:
          print('Invalid player:', player)
          assert False
  return games


def FetchGame(cachedir, game_id):
  url = SHOWGAME_BASE_URL + "?ga=" + urllib.parse.quote(game_id)
  cache_filename = os.path.join(cachedir, urllib.parse.quote_plus(url))
  if os.path.exists(cache_filename):
    # Read content from cache
    with open(cache_filename, 'rt') as f:
      text = f.read()
  else:
    # Fetch from internet
    with urllib.request.urlopen(url) as f:
      text = str(f.read(), 'utf-8')
    # Write content to cache
    with open(cache_filename, 'wt') as f:
      f.write(text)

  # Parse content (this should be in a separate function but whatever)
  m = GAME_RE.search(text)
  if m is None:
    print('Skipping game', game_id, 'url:', url)
    return None
  users, moves = map(json.loads, m.groups())
  assert len(users) == 2
  assert all(isinstance(elem, str) for elem in users)
  assert all(isinstance(elem, str) for elem in moves)
  # In the CSV, players never have leading or trailing whitespace, but in the
  # JSON data they sometimes do (see the user called "Anonymous "), so I strip
  # the usernames here to make user names match the CSV.
  users = [user.strip() for user in users]
  solution = None
  if moves and len(moves[-1]) == 81:
    solution = moves.pop()
  return (users, moves, solution)


def WriteAnnotatedCsv(filename, games):
  with open(filename, 'wt', newline='') as f:
    w = csv.writer(f, lineterminator='\n')
    w.writerow(('Game', 'Round', 'IsSwiss', 'User1', 'Score1', 'Status1', 'User2', 'Score2', 'Status2', 'Moves', 'Solution'))
    for game in games:
      w.writerow((game.game_id, game.round, game.is_swiss, game.user1, game.score1, game.status1, game.user2, game.score2, game.status2, game.moves, game.solution))


if __name__ == '__main__':
  if len(sys.argv) != 4:
    print('Usage: %s <cachedir> <competition-csv> <games-csv>' % (sys.argv[0],))
    sys.exit(1)
  else:
    _, cachedir, competition_csv, games_csv = sys.argv
    assert os.path.isdir(cachedir)
    games = ReadCompetitionCsv(competition_csv)
    for i, game in enumerate(games):
      print('%s (%d / %d)' % (game.game_id, i + 1, len(games)), file=sys.stderr)
      assert game.moves is None
      assert game.solution is None
      details = FetchGame(cachedir, game.game_id)
      if details is not None:
        users, moves, solution = details
        assert users == [game.user1, game.user2]
        game.moves = ' '.join(moves)
        game.solution = solution
    WriteAnnotatedCsv(games_csv, games)
