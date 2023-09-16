#!/usr/bin/env python3

# Fetches my player logs from the CodeCup server.
#
# The easiest way to use this is with the fetch-competition-playerlogs.sh
# wrapper script.

import html
import re
import os
import os.path
import sys
import urllib.parse
import urllib.request

# This text is expected on the page iff. the game id is valid.
GAME_REPORT_TEXT = 'Game report'

# This text is expected on the page iff. we are logged in correctly.
LOGGED_IN_TEXT = 'Player messages'

PLAYERLOG_RE = re.compile(r'<b>Player messages:</b><br>(.*)<br>')
TIME_USED_RE = re.compile(r'<b>Total time used:</b> ([0-9.]*) seconds<br>')

# Enable this to print debug info about HTTP requests:
if False:
  urllib.request.install_opener(
    urllib.request.build_opener(
      urllib.request.HTTPHandler(debuglevel=1),
      urllib.request.HTTPSHandler(debuglevel=1)))


def FetchGame(game_id, codecupid, filename):
  if os.path.exists(filename):
    print(f'Skipping game {game_id}; file already exists: {filename}')
    return True

  # Fetch from server
  url = 'https://www.codecup.nl/showgame.php?ga=' + urllib.parse.quote(game_id)
  request = urllib.request.Request(url)
  request.add_header('Cookie', 'codecupid=' + urllib.parse.quote(codecupid))
  with urllib.request.urlopen(request) as f:
    response_text = str(f.read(), 'utf-8')
  if GAME_REPORT_TEXT not in response_text:
    print(f'Missing game report header! Maybe invalid game URL: {url}')
    return False
  if LOGGED_IN_TEXT not in response_text:
    print('Missing player messages in response! Are you logged in correctly?')
    return False
  time_used = TIME_USED_RE.search(response_text).group(1)
  player_log = PLAYERLOG_RE.search(response_text).group(1)
  player_log = html.unescape(player_log)
  player_log = player_log.replace('<br>' , '\n')
  player_log = player_log.replace('\u00a0' , ' ')  # non-breaking space
  with open(filename, 'wt') as f:
    f.write(player_log)
    print(f'# Total time used: {time_used} seconds', file=f)
    print(f'Downloaded game {game_id} to output file: {filename}')
  return True


if __name__ == '__main__':
  if len(sys.argv) < 3:
    print('Usage: %s <output-dir> <game-ids>...' % (sys.argv[0],))
    sys.exit(1)

  codecupid = os.environ.get('CODECUPID', '')
  if not codecupid:
    print('CODECUPID environmental variable is not set!')
    sys.exit(1)

  output_dir = sys.argv[1]
  if not os.path.isdir(output_dir):
    print('Not a directory:', output_dir)
    sys.exit(1)

  for gameid in sys.argv[2:]:
    output_filename = os.path.join(output_dir, 'game-' + gameid + '.txt')
    if not FetchGame(gameid, codecupid, output_filename):
      sys.exit(1)
