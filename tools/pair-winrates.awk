#!/usr/bin/awk -f

# Run this on a competition results.txt file to calculate the winrate for
# each pair of players.
#
# It will print a table like this:
#
#       foo  bar  baz
#  foo    -  85%  93%
#  bar   15%   -  42%
#  baz    7% 58%   -
#
# which means that `foo` won 85% of its matches against `bar`.
#
# This script currently does not distinguish between player 1 and player 2
# victories (which could be interesting, too).

BEGIN {
    player_count = 0;
}

function addPlayer(name) {
    if (!(name in player_index)) {
        player_index[name] = player_count;
        player_names[player_count] = name;
        player_count += 1;
    }
    return player_index[name];
}

function addMatch(winner, loser) {
    addPlayer(winner);
    addPlayer(loser);

    wins[winner "/" loser] += 1
    matches[winner "/" loser] += 1
    matches[loser "/" winner] += 1
}

$4 == "WIN" { addMatch($2, $3) }
$5 == "WIN" { addMatch($3, $2) }

END {
    # Print column header
    printf("%20s ", "");
    for (j = 0; j < player_count; j += 1) {
        loser = player_names[j];
        printf("%20s ", player_names[j]);
    }
    printf("\n");
    # Print rows
    for (i = 0; i < player_count; i += 1) {
        winner = player_names[i];
        printf("%20s ", winner);
        for (j = 0; j < player_count; j += 1) {
            if (i == j) {
                printf("%20s ", "-");
            } else {
                loser = player_names[j];
                printf("%19.2f%% ", 100 * wins[winner "/" loser] / matches[winner "/" loser]);
            }
        }
        printf("\n");
    }
}
