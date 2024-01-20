This repository contains the source code for my entry in the 2024 CodeCup,
playing Sudoku (not the famous puzzle, but a two-player game inspired by it).

See https://www.codecup.nl/ and soon https://archive.codecup.nl/2024/ for
more information about the competition.


SUMMARY OF STRATEGY

The strategy employed by the player is essentially:

 1. Try to find a complete set of solutions.

    If the solution set is too large (more than 100,000 in my official
    submission) or it takes too much work to enumerate them, play randomly
    instead (there is a small risk of making an invalid move, but it's
    unlikely early in the game).

    Note that this step needs to be completed only once per game.

 2. If we have a complete solution set, then find an optimal move using
    exhaustive depth-first search. If we cannot complete the depth-first search
    in a reasonable amount of time (I use 33% of the time remaining), play
    randomly instead.

    In the depth first search, I first calculate for each open cell the set of
    possible digits (as the union of digits that occur in that cell across
    remaining solutions). If a cell has only 1 possible digit remaining, then
    we may not play there, because it would not reduce the solution set.
    Otherwise, simply try filling in each possible digit in each possible cell,
    calculate the remaining subset, and recurse.

    The usual rules of minimax search apply: a position is winning iff. there
    exists a move that leads to a losing position (for the opponent). It follows
    that a position is losing iff. all moves lead to a winning position,
    including the special case where there is only 1 solution remaining, so no
    moves are possible.


I used two optimizations to make this algorithm work well:

  1. Move ordering: in the depth-first search, try moves first that reduce
     the solution set the most (i.e., the remaining subset is as small as
     possible).

  2. Transposition table: I use a hash table to cache the result for repeated
     states. This greatly reduces the number of recursive calls, but makes them
     more expensive.


For more random notes (some of which may be outdated), see NOTES.txt



DEPENDENCIES

 - C/C++
 - GNU Make



BUILDING

To build all binaries, simply run make:

% make -j4

Output files are in output/

To build just debug/release binaries:

% make debug  # or release

To create the combined player source file at output/player-combined.cc:

% make combined

To only build e.g. a release build of the solver:

% make -f Makefile.release solver
