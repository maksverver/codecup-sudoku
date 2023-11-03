#!/bin/bash
#
# This tool uses the solver to identify mistakes from a game transcript.
#
# It prints the incorrect move numbers to stdout (one per line), from end to
# beginning.
#
# Odd numbers indicate a mistake by player 1.
# Even numbers indicate a mistake by player 2.
#

set -e -E -o pipefail

# The max_work parameter determines how deeply the game is analyzed.
# When the work limit is exceeded, analysis is stopped, and further mistakes
# are not identified.
SOLVER='output/release/solver --analyze-max-work=1e9'

basedir=$(dirname $(readlink -f "$0"))
cd "${basedir}"/..

moves=$1

if [ $# != 1 ]; then
  echo "Usage: $0 <transcript>"
fi

if ! echo "$moves" | grep -E -q '^([A-I][a-i][1-9][^[:alnum:]])*([A-I][a-i][1-9])?[!]$'; then
  echo "Invalid transcript!" >&2
  exit 1
fi

moves=($(echo "$moves" | grep -E -o '[A-I][a-i][1-9]'))

if ! $SOLVER "${moves[*]}" | grep -qF 'Solution is unique!'; then
  echo "Transcript does not end with unique solution!" >&2
  exit 1
fi

turn=${#moves[@]}  # 0-based
winning=1
while true; do
  turn=$((turn - 1))
  last_move=${moves[$turn]}
  moves=(${moves[@]:0:$turn})
  result=$($SOLVER "${moves[*]}" | grep -E 'Outcome|Analysis incomplete|Solution is unique')
  if [ "$result" = "Analysis incomplete!" ]; then
    echo "$result" >&2
    exit 0
  else
    if [ "$result" = "Solution is unique!" ]; then
      outcome=1
    elif echo "$result" | grep -Fq 'Outcome: WIN'; then
      outcome=1
    else
      outcome=0
    fi
    if [ $outcome != $winning ]; then
      echo "Player $((turn % 2 + 1)) blundered on move $((turn + 1)) ($last_move)" >&2
    fi
    winning=$((1 - $outcome))
  fi
done
