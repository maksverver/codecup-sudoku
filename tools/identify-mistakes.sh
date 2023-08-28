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
SOLVER='output/release/solver --analyze_max_work=1e9'

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

if [ $(( ${#moves} % 4 )) -eq 1 ]; then
  # Transcript ends with "!" to claim solution is already unique
  moves=${moves::-1}
  turn=$((${#moves} / 4))  # 0-based!
  echo "$turn"
  echo "Player $(( (turn - 1) % 2 + 1)) blundered on move $((turn))" >&2
fi

if ! $SOLVER "$moves" | grep -qF 'Solution is unique!'; then
  echo "Transcript does not end with unique solution!" >&2
  exit 1
fi

winning=1
while true; do
  moves=${moves::-4}
  turn=$((${#moves} / 4))  # 0-based!
  result=$($SOLVER "$moves" | grep -E 'Outcome|Analysis incomplete')
  if [ "$result" = "Analysis incomplete!" ]; then
    echo "$result" >&2
    exit 0
  else
    if echo "$result" | grep -Fq 'Outcome: WIN'; then
      outcome=1
    else
      outcome=0
    fi
    if [ $outcome != $winning ]; then
      echo "$((turn + 1))"
      echo "Player $((turn % 2 + 1)) blundered on move $((turn + 1))" >&2
    fi
    winning=$((1 - $outcome))
  fi
done
