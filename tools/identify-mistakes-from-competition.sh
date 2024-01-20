#!/bin/bash
#
# Calculates mistakes in a competition.
#
# Prints line of the form:
#
# <game_id> <1-based turn> <user name>

basedir=$(dirname $(readlink -f "$0"))
cd "${basedir}"/..

if [ $# != 1 ]; then
  echo "Usage: $0 <games.csv>"
  exit 1
fi

competition_csv=$1

if [ ! -f "$competition_csv" ]; then
  echo "Missing input file: $competition_csv"
  exit 1
fi

grep '!' "$competition_csv" | while IFS=, read game round is_swiss user1 score1 status1 user2 score2 status2 moves solution
do
  echo $game $user1 $status1 $user2 $status2 $moves >&2
  mistakes=$(tools/identify-mistakes.sh "$moves" | tac)
  for turn in $mistakes; do
    if [ $(($turn % 2)) -eq 1 ]; then
      failed_user=${user1}
    else
      failed_user=${user2}
    fi
    printf "%s\t%d\t%s\n" "$game" "$turn" "$failed_user"
  done
done
