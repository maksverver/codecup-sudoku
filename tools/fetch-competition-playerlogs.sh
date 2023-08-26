#!/bin/bash

# Script to fetch my player logs from the CodeCup server after a competition.
#
# This is just a wrapper around fetch-playerlogs.py which does the actual
# fetching.
#
# Reads the competition CSV from competition-results/<competition-id>.csv
# Stores the player logs to playerlogs/<competition-id>/
#
# Needs the value of the `codecupid` cookie which can be found in the browser
# after logging in, e.g., in Chrome, under Application > Storage > Cookies.

set -e -E -o pipefail

basedir=$(dirname $(readlink -f "$0"))
cd "${basedir}"/..

if [ $# != 2 ]; then
  echo "Usage: $0 <competition id> <user name>"
  exit 1
fi

competition_id=$1
user_name=$2

input_csv=competition-results/${competition_id}.csv
output_dir=playerlogs/${competition_id}/

if [ ! -f "${input_csv}" ]; then
  echo "Missing input file: ${input_csv}"
  exit 1
fi

# Find the ids of games the user participated in.
# Fails silently if there are no matches! (This is default grep behavior.)
game_ids=($(grep -F "$user_name" "$input_csv" | cut -d',' -f1))

# Read the value of the codecupid cookie.
echo -n 'CodeCupId: '
read codecupid

# Actually fetch the player logs. The codecupid is passed as an environmental
# variable instead of a command line arugment to avoid leaking it in the
# process table.
CODECUPID=$codecupid tools/fetch-playerlogs.py "$output_dir" "${game_ids[@]}"
