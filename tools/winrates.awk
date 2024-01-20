#!/usr/bin/awk -f

# Run this on a games.csv file to calculate P1 and P2 winrates.
# 
# Note: this only counts games that ended regularly, i.e., with a unique
# solution. This excludes games where the loser made an invalid move or
# ran out of time.

BEGIN {
  FS=","
}

$6 == "WIN" && $9 == "LOSE" { p1 += 1; n += 1 }
$6 == "LOSE" && $9 == "WIN" { p2 += 1; n += 1 }

END {
  print "P1 wins: "p1"/"n" = "p1/n*100"%"
  print "P2 wins: "p2"/"n" = "p2/n*100"%"
}
