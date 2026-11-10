# SolvedIn audit (2026-11-09)

Everything in this file was run on this machine; commands are given so you can repeat them. "Archived puzzles" are 795 real puzzles collected earlier from three public archive sites; **they are not shipped in this repository** (see LIMITATIONS.md), so the tables in sections 3-5 cannot be reproduced from this repository alone. Section 1 and the samples in section 2 can.

## 1. Answer-data audit (can the solver see answers?)

- **Runtime inputs.** The CLI reads only the files named on its command line (`--dict`, `--lexicon`, the board/clue file) plus two embedded defaults: a 1,253-word list in `src/trie.cpp` (identical to `fixtures/default_lexicon.txt`) and a 10-category Pinpoint lexicon in `src/pinpoint.cpp` (identical to `fixtures/pinpoint_lexicon.txt`). A grep of `src/` finds no other file or environment access except the board/dictionary/lexicon loaders (`ifstream` in the `*_board.cpp`, `trie.cpp`, `pinpoint.cpp`).
- **Shipped fixtures vs archived puzzles.** Compared by normalised text against every archived input: 0 shipped fixtures equal an archived board, and 0 shipped Pinpoint clue sets equal an archived clue set.
- **Embedded word list.** 244 of 545 archived Wend/Crossclimb answer words (45 %) are in the 1,253-word default list, versus 56 of 545 (10 %) for random same-length words from the large system list. That is what a list of *common* words looks like (answers are common words), not evidence of copying: the file was written on 2026-11-08 12:38, about 4.5 hours before the first archive was fetched (17:11 the same day). It is only used when you give no `--dict`; **every scored run below passed an explicit, frozen dictionary** (`/usr/share/dict/words`, upper-cased, 2-12 letters, 197,014 words).
- **Embedded Pinpoint lexicon.** 10 hand-written categories (89 terms); 4 of those terms (AMETHYST, BASS, ESPRESSO, HARD) also occur as archived clue words, and "Coffee Drinks" shares a word with one archived category ("Words that come before coffee"). No category equals an archived one. Created 2026-11-08 12:52, before the first fetch. **Decision for you:** "Computer Storage Drives" and "Types of Necklines" sound like real Pinpoint categories; I could not verify their origin.
- **Evaluation hygiene.** Cases were fed to the CLI as bare board files; for the census in section 3 I re-ran every case from **anonymised copies** (`c0001.txt`, ...) so no puzzle id appears in the path. Gold answers, archived solutions and answer-derived dictionaries were never passed to the solver (the "answer-augmented" rerun of an earlier report is a separate diagnostic and is not used here).
- **Not covered:** I cannot rule out that a third-party archive contains errors, or that `/usr/share/dict/words` happens to contain an answer (it is a general dictionary, not answer-derived).

## 2. The eight games: sample, algorithm, pruning, verdict

### Queens
**Rules.** Colour-region grid; one queen per row, column and region, no two touching (including diagonally).

**Algorithm.** Bitset search state: unassigned rows/columns/regions and an `available` cell set (multi-word bitset, bit = `r*N+c`). Each node picks the **most constrained unit** (fewest available cells, MRV) from rows, columns and regions, orders its cells **least-constraining first** (LCV: cells that block the fewest other available cells), places one, removes its row/column/region and the 8 neighbours from `available`, and recurses.

**Pruning.** **Forward checking:** before descending, `bitset_has_wipeout` tests whether any unassigned row, column or region has no available cell left; if so the branch is rejected without a node. A unit with no cell can never be satisfied because placements only ever remove cells. Undo restores the saved bitsets.

Input (`fixtures/valid_8x8_unique.txt`):
```text
8
0 2 1 0 0 1 0 1
0 1 0 1 0 1 1 1
0 0 2 2 0 2 2 0
0 0 3 1 0 0 0 0
4 1 4 0 4 4 0 0
0 5 0 0 0 5 0 0
6 0 6 0 6 0 0 0
7 7 7 0 0 7 0 7
```
Command and output:
```text
$ solvedin queens solve fixtures/valid_8x8_unique.txt --solver bitset --check-unique --stats
Unique solution found:
Q . . . . . . .
. . . Q . . . .
. . . . . . Q .
. . Q . . . . .
. . . . . Q . .
. Q . . . . . .
. . . . Q . . .
. . . . . . . Q
Queen positions (row, col):
  (0, 0)
  (1, 3)
  (2, 6)
  (3, 2)
  (4, 5)
  (5, 1)
  (6, 4)
  (7, 7)

--- Search Metrics ---
Solver variant: bitset
Nodes visited : 10
Elapsed time  : 0.018 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Solves.** 100/100 archived boards valid and verified unique (median 16 nodes, 0.01 ms). Synthetic giant boards are seed- and platform-dependent: 128x128 seed 42 solves in 737 nodes on macOS but not on Linux; 128x128 seed 7 exhausts a 3M-node budget.

### Mini Sudoku
**Rules.** 6x6 grid, digits 1-6, 2x3 boxes.

**Algorithm.** Per-cell 6-bit candidate masks. Propagation to a fixed point with **naked singles** (a cell with one candidate) and **hidden singles** (a digit with one place in a row/column/box). If propagation stalls, branch on the cell with the fewest candidates (MRV). There is no AC-3.

**Pruning.** A cell with an empty mask, or a unit with a digit that has no place, is a contradiction and the branch is abandoned; singles are forced by the rules so propagation never removes a solution.

Input (`fixtures/sudoku_branching.txt`):
```text
6
. . 5 1 . .
. 1 . . . 6
5 . . 6 4 .
. 6 . 5 . .
2 . . . . 5
. . . . 1 .
```
Command and output:
```text
$ solvedin sudoku solve fixtures/sudoku_branching.txt --check-unique --stats
Unique solution found:
6 2 5 | 1 3 4
4 1 3 | 2 5 6
------+-------
5 3 2 | 6 4 1
1 6 4 | 5 2 3
------+-------
2 4 1 | 3 6 5
3 5 6 | 4 1 2

--- Search Metrics ---
Nodes visited : 4
Elapsed time  : 0.007 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Solves.** 100/100 valid and unique, 1 node each (all solved by propagation).

### Tango
**Rules.** 6x6 grid of Sun/Moon: equal counts per row/column, no three in a row, `=`/`x` edge clues between neighbours.

**Algorithm.** 2-bit domain per cell. Propagation applies the count rule (a full row/column of one symbol forces the rest), the no-three rule, and edge clues (`=` copies, `x` flips). Branches on the **first undecided cell** (no MRV).

**Pruning.** Any cell whose domain empties, any row/column exceeding N/2 of one symbol, or any three-in-a-row is a contradiction; each rule is a necessary condition of a valid grid.

Input (`fixtures/tango_edge_clue_dependent.txt`):
```text
# 6x6 Tango where horizontal clue at row 3 col 1 '=' determines the unique solution
6
GRID
S . . M . S
M . . S . M
S . . . . .
. . . . S .
S M S . . .
. . . . S M
HORIZONTAL
. . . . .
. . . . .
. . . . .
. = . . .
```
Command and output:
```text
$ solvedin tango solve fixtures/tango_edge_clue_dependent.txt --check-unique --stats
Unique solution found:
S M S M M S
M S M S S M
S M M S M S
M S S M S M
S M S M M S
M S M S S M

--- Search Metrics ---
Nodes visited : 4
Elapsed time  : 0.027 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Solves.** 100/100 valid and unique (median 7 nodes, p95 34, max 131).

### Zip
**Rules.** Draw one path through every cell, orthogonally, around walls, visiting numbered checkpoints in order.

**Algorithm.** DFS from checkpoint 1; next moves ordered by **Warnsdorff** (fewest onward moves first).

**Pruning.** At each step a BFS over unvisited cells checks that all of them are still reachable from the path head, and a degree test rejects any unvisited cell with fewer than two free neighbours (one for the final cell); also rejects when the next checkpoint is unreachable or visited out of order.

Input (`fixtures/zip_checkpoint_order.txt`):
```text
# 3x3 Zip puzzle testing checkpoint ordering (1 -> 2 -> 3 -> 4) and vertical walls
3 3
GRID
1 . 3
. . .
2 . 4
WALLS_V
| .
. .
. |
```
Command and output:
```text
$ solvedin zip solve fixtures/zip_checkpoint_order.txt --check-unique --stats
Unique solution found:
--- Path Step Grid ---
1 6 7
2 5 8
3 4 9
Path (9 steps):
  (0, 0) -> (1, 0) -> (2, 0) -> (2, 1) -> (1, 1) -> (0, 1) -> 
  (0, 2) -> (1, 2) -> (2, 2)

--- Search Metrics ---
Nodes visited : 11
Elapsed time  : 0.008 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Solves.** 100/100 valid and unique (median 263 nodes, p95 6,026, max 42,121; max 50 ms). Cost grows quickly with board size and few checkpoints.

### Patches
**Rules.** Tile the board with solid rectangles, each containing exactly one clue cell and satisfying its size/shape clue.

**Algorithm.** All legal rectangles per clue are enumerated once, each with a 128-bit cell mask. The search picks the unused clue with the fewest candidates still disjoint from the covered mask (**MRV**) and tries them in order.

**Pruning.** (1) A clue with no compatible candidate => backtrack. (2) **Cell-coverage prune (added in this audit):** if any uncovered cell lies outside the union of all compatible candidates of all unused clues => backtrack. Both are necessary conditions of a complete exact cover.

Input (`fixtures/patches_valid_unique.txt`):
```text
# 6x6 Patches puzzle exercising squares, tall, wide, freeform, and partial clues
6 6
GRID
4S  .  6W   .   .  4T
 .  .   .   .   .   .
4F  .   .   4   .   .
 .  .   ?   .   .   .
4S  .   T  6W   .   .
 .  .   .   .   .   .
```
Command and output:
```text
$ solvedin patches solve fixtures/patches_valid_unique.txt --check-unique --stats
Unique solution found:
--- Patch Tiling Grid ---
A A B B B C
A A B B B C
D D I E E C
D D I E E C
F F G H H H
F F G H H H
Patches (9 total):
  Patch A [clue '4S' at (0, 0), size 4]: (0,0) (0,1) (1,0) (1,1)
  Patch B [clue '6W' at (0, 2), size 6]: (0,2) (0,3) (0,4) (1,2) (1,3) (1,4)
  Patch C [clue '4T' at (0, 5), size 4]: (0,5) (1,5) (2,5) (3,5)
  Patch D [clue '4F' at (2, 0), size 4]: (2,0) (2,1) (3,0) (3,1)
  Patch E [clue '4' at (2, 3), size 4]: (2,3) (2,4) (3,3) (3,4)
  Patch F [clue '4S' at (4, 0), size 4]: (4,0) (4,1) (5,0) (5,1)
  Patch G [clue 'T' at (4, 2), size 2]: (4,2) (5,2)
  Patch H [clue '6W' at (4, 3), size 6]: (4,3) (4,4) (4,5) (5,3) (5,4) (5,5)
  Patch I [clue '?' at (3, 2), size 2]: (2,2) (3,2)

--- Search Metrics ---
Nodes visited : 11
Elapsed time  : 0.011 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Solves.** 100/100 archived boards valid and unique after the audit fix (before: 99, one 10 s timeout on #151); max 1,365 nodes, 0.11 ms.

### Wend
**Rules.** Cover every cell of the letter grid with orthogonal word paths of the given lengths, each spelling a dictionary word.

**Algorithm.** Stage 1: a trie walk enumerates every dictionary-word path of an allowed length (paths spelling the same word from different cells are kept separate). Stage 2: exact cover - pick the uncovered cell with the fewest candidate paths (MRV) and try them.

**Pruning.** A trie prefix that is not in the dictionary cannot become a word; a cell with no remaining path => backtrack; lengths are tracked as a multiset.

Input (`fixtures/wend_valid_unique.txt`):
```text
# 3x3 Wend puzzle with a unique cover of three 3-letter words
3 3
GRID
F O R
V A N
P I G
LENGTHS: 3 3 3
```
Command and output:
```text
$ solvedin wend solve fixtures/wend_valid_unique.txt --check-unique --stats
Unique solution found:
Wend Solution (3 words):
  [1] FOR (3): (0,0) (0,1) (0,2)
  [2] VAN (3): (1,0) (1,1) (1,2)
  [3] PIG (3): (2,0) (2,1) (2,2)
Cover Map:
  1 1 1 
  2 2 2 
  3 3 3 

--- Search Metrics ---
Nodes visited : 4
Elapsed time  : 0.008 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Depends on the dictionary.** With a 197k-word list: 76/95 archived boards valid (69 verified unique, 7 valid but not unique), 19 `NO_SOLUTION` only because an answer word is missing from the list, 1 valid alternate to the published answer. The embedded 1,253-word list is far too small for real boards.

### Crossclimb
**Rules.** Order the rung words so that neighbouring rungs differ by one letter, with top and bottom words.

**Algorithm.** Build a graph over the **rung words you type in** (edge = Hamming distance 1); build start/end pools from the given words, supplied candidates, or the dictionary; DFS over unused rungs (Hamiltonian path).

**Pruning.** Only rungs one letter apart are extended; every rung is used once; an empty endpoint pool => `NO_SOLUTION`.

Input (`fixtures/crossclimb_valid_unique.txt`):
```text
# 4-letter Crossclimb puzzle with a unique ladder
WORD_LENGTH: 4
START: COLD
RUNGS:
CARD
WARD
CORD
END: WARN
```
Command and output:
```text
$ solvedin crossclimb solve fixtures/crossclimb_valid_unique.txt --check-unique --stats
Unique solution found:
Crossclimb Ladder (5 rungs):
  [0] COLD (START)
  [1] CORD
  [2] CARD
  [3] WARD
  [4] WARN (END)

--- Search Metrics ---
Nodes visited : 3
Elapsed time  : 0.002 ms
Budget status : WITHIN_BUDGET
```

**Verdict.** **Supplied-endpoint mode solves** when the answer words are typed in and both endpoints are in the dictionary: 83/100 archived puzzles valid and unique, 17 failures only because an endpoint word was missing from the list. **Hidden-endpoint mode does not identify a ladder**: 0/100 unique (99 ambiguous, 1 no solution). It never reads clues; see section 5 for the clues-alone prototype.

Hidden endpoints with supplied candidate lists (`fixtures/crossclimb_missing_endpoints.txt`):
Input (`fixtures/crossclimb_missing_endpoints.txt`):
```text
# 4-letter Crossclimb puzzle with missing start and end endpoints and clue candidates
WORD_LENGTH: 4
START: ?
START_CANDIDATES: COLD BOLD
RUNGS:
CARD
WARD
CORD
END: ?
END_CANDIDATES: WARN WARM
```
Command and output:
```text
$ solvedin crossclimb solve fixtures/crossclimb_missing_endpoints.txt --stats
Multiple solutions found (ambiguous ladder, at least 2):

--- Solution 1 ---
Crossclimb Ladder (5 rungs):
  [0] COLD (START)
  [1] CORD
  [2] CARD
  [3] WARD
  [4] WARN (END)

--- Solution 2 ---
Crossclimb Ladder (5 rungs):
  [0] COLD (START)
  [1] CORD
```

### Pinpoint
**Rules.** Guess the category that links up to five revealed clues.

**Algorithm.** Not a search. Keyword overlap between each clue and each category's term list in a lexicon *you supply*; confidence = supported clues / clues; abstain (`UNKNOWN`) below 0.50.

**Pruning.** None (linear scan). Categories with zero supported clues are dropped.

Input (`fixtures/pinpoint_clues_drives.txt`):
```text
# Pinpoint clues for Computer Storage Drives
CLUES:
FLOPPY
HARD
COMPACT
FLASH
SOLID STATE
```
Command and output:
```text
$ solvedin pinpoint guess fixtures/pinpoint_clues_drives.txt
Pinpoint Guesser Result (5 clues provided):
Clues: [FLOPPY, HARD, COMPACT, FLASH, SOLID STATE]

STATUS: CONFIDENT GUESS
  #1 Computer Storage Drives (confidence: 100.0%)
     Supported (5): FLOPPY HARD COMPACT FLASH SOLID STATE 
```

A clue set whose category is not in the lexicon:
Input (`fixtures/pinpoint_clues_weak.txt`):
```text
# Pinpoint clues with weak / unknown evidence (out-of-lexicon categories)
CLUES:
SPACESHIP
QUANTUM
SUBMARINE
TELEPORTATION
```
Command and output:
```text
$ solvedin pinpoint guess fixtures/pinpoint_clues_weak.txt
Pinpoint Guesser Result (4 clues provided):
Clues: [SPACESHIP, QUANTUM, SUBMARINE, TELEPORTATION]

STATUS: UNKNOWN (evidence is too weak to make a confident guess)
```

**Verdict.** **Retrieval only.** It can name a category only if it is already in the lexicon; it scored 0 of 46 on puzzles whose categories were excluded from the lexicon (section 5).


## 3. Fresh-input runs on archived puzzles (player-visible inputs only, independent validators)

Protocol: each case file contains only what a player sees (grid/clues; for Wend and Crossclimb a frozen 197,014-word dictionary passed with `--dict`; for Crossclimb the five answer words and, in supplied-endpoint mode, both endpoints). The solver was not tuned on these puzzles, except that Patches #151 was profiled in the previous session. Outputs were checked by independent **Python** rule checkers (not the C++ validators). Budget 10 s per case. The census re-ran every case from anonymised copies with `--check-unique`.

Outcome counts (frozen dictionary; 795 unique puzzles, 100 per game except Wend 95):

| Game | Valid | Verified unique | Valid, not unique | Ambiguous (no single answer) | Timeout | Failure |
|:--|:-:|:-:|:-:|:-:|:-:|:-:|
| Queens | 100/100 | 100 | 0 | 0 | 0 | 0 |
| Mini Sudoku | 100/100 | 100 | 0 | 0 | 0 | 0 |
| Tango | 100/100 | 100 | 0 | 0 | 0 | 0 |
| Zip | 100/100 | 100 | 0 | 0 | 0 | 0 |
| Patches (after fix) | 100/100 | 100 | 0 | 0 | 0 | 0 |
| Patches (before fix) | 99/100 | 99 | 0 | 0 | 1 | 0 |
| Wend | 76/95 | 69 | 7 | 0 | 0 | 19 (answer word not in dictionary) |
| Crossclimb, supplied endpoints | 83/100 | 83 | 0 | 0 | 0 | 17 (endpoint not in dictionary) |
| Crossclimb, hidden endpoints, dictionary only | 0/100 | 0 | 0 | 99 | 0 | 1 (no solution) |

"Valid" for the five grid games = the output satisfies every rule; where a published answer existed (64 of 100 Queens, Zip and Patches boards; all of Sudoku and Tango) it matched, and the rest are valid without a reference. Wend: 75 match the published cover, 1 is a valid alternate.

Search nodes and solver time (after the Patches fix; `scripts/census.py` in the development repository):

| set | n | nodes median / p95 / max | search ms median / p95 / max | classes |
|:--|:-:|:-:|:-:|:--|
| queens | 100 | 16 / 74 / 190 | 0.011 / 0.04 / 0.082 | {'valid_no_reference': 36, 'match': 64} |
| sudoku | 100 | 1 / 1 / 1 | 0.003 / 0.004 / 0.004 | {'match': 100} |
| tango | 100 | 7 / 34 / 131 | 0.026 / 0.073 / 0.265 | {'match': 100} |
| zip | 100 | 263 / 6026 / 42121 | 0.261 / 7.226 / 49.937 | {'valid_no_reference': 36, 'match': 64} |
| patches | 100 | 25 / 162 / 1365 | 0.019 / 0.074 / 0.109 | {'valid_no_reference': 36, 'match': 64} |
| wend | 95 | 5 / 9 / 17 | 0.121 / 0.27 / 0.693 | {'match': 75, 'no_solution__reference_word_not_in_frozen_dictionary': 19, 'valid_alternate': 1} |
| crossclimb:given_endpoints | 100 | 5 / 5 / 15 | 0.009 / 0.018 / 0.043 | {'no_solution__reference_word_not_in_frozen_dictionary': 17, 'match': 83} |
| crossclimb:hidden_endpoints | 100 | 5 / 70 / 210 | 0.164 / 0.341 / 0.494 | {'multiple_solutions_reported': 99, 'no_solution': 1} |

Before the Patches fix (same run, same machine): Patches nodes median/p95/max = 149 / 803,574 / 1,273,260,032 (budget hit), search ms 0.02 / 6.7 / 10,000. The other seven rows were unchanged.

## 4. Profiling and fixes

- **Patches #151** - two profiles, two fixes; see ALGORITHMS.md 6.8. Final: 1,388,596,707 nodes / 10.7 s -> 300 nodes / 0.12 ms, identical answer, identical output on 114 boards in both solve modes.
- **Other budget-hitting cases.** In the archived benchmark nothing else times out (slowest remaining: Zip, 50 ms). The only other budget hits are synthetic Queens boards in `giant-queens` (32x32 seed 42 needs 593,593 nodes / 0.5 s but the benchmark budget is 200,000; 128x128 seed 7 does not finish in 3,000,000 nodes / 24 s). A profile of 128x128 seed 7 shows >95 % of time inside the bitset search loop (`for_each_set_bit` and the search function) and negligible allocation, so there is no cheap proven bottleneck to fix; it would need stronger propagation. Real Queens boards are at most 9x9. Not fixed.

## 5. Clues only (Crossclimb) and unseen categories (Pinpoint)

Both prototypes are Python, macOS-only, read system dictionary/embedding assets and are **not part of this repository**. Frozen chronological holdouts (IDs earlier than the development block), zero paid API calls; answers, clue-to-word mappings and derived statistics were kept out of the solver.

**Crossclimb from clues alone (48 holdout puzzles; re-run on 2026-11-09 with the same frozen holdout and weights; all 48 per-puzzle outcomes are identical to the earlier frozen run):** full ladder ranked first 8/48 (17 %), in the top 3 9/48. The solver answered on 2 puzzles (both correct) and abstained on 46. Per-clue candidate coverage: 210 of 240 published words (88 %) were in the candidate lists, but all five words of a puzzle were covered for only 24/48. Failures: 24 missing clue candidate, 14 ladder ranking (all words present), 2 endpoints, 0 search truncation. Supplied-endpoint mode (section 3) is a different task: it is given the answer words.

**Pinpoint on categories excluded from the lexicon (46 holdout puzzles; re-run today, numbers identical to the earlier run):**

| Clues shown | Correct category generated | Top-1 | Top-3 | Abstained | Confident and wrong |
|:-:|:-:|:-:|:-:|:-:|:-:|
| 1 | 5 | 0 | 2 | 46 | 0 |
| 2 | 5 | 4 | 5 | 46 | 0 |
| 3 | 8 | 8 | 8 | 41 | 0 |
| 4 | 8 | 8 | 8 | 39 | 0 |
| 5 | 8 | 8 | 8 | 39 | 0 |

(Top-1/Top-3 describe the ranked list whatever the solver abstained on.) All 8 hits are "Words that come before/after X" categories; 0 of the other 35 were recovered. **The C++ lexicon solver on the same 46 puzzles: 0 correct at every clue count**, with confident wrong answers on some (development-built lexicon: 9/21/2/2/0 at 1-5 clues; seed lexicon: 2/2/0/0/0). For reference, archive retrieval (lexicon built from the scored puzzles, i.e. lookup) gets 87/99/100/100/100 of 100 top-1 - that is not accuracy on unseen puzzles.

**Why the first re-run stalled, and the fix.** The ladder search keeps the best B partial chains per state, but pruned each state only *after* expanding it, so work grew with the unpruned chain count. On puzzle #812 (dense graph of 4-letter candidates) it ran for 40+ minutes. Pruning *before* expansion (`XC_BEAM=1`, research prototype only) finishes #812 in 9 s and, on all 48 holdout puzzles, gives the same class and status as the exhaustive run (no differences). This change was made after seeing the holdout, so treat it as an engineering fix verified for equivalence, not a re-tuned result.

## 6. What to do next
Remaining weaknesses, in order of impact: Crossclimb and Pinpoint do not solve from clues in general; Wend needs a broad dictionary and then returns more non-unique answers; Queens/Zip can blow up on large synthetic boards.

## 7. Fresh spot-check of every advertised mode (2026-11-10)

**Inputs.** Puzzlegames.fun had no puzzles newer than the corpus (the next IDs of every game returned 404 on 2026-11-10), so I fetched IDs that no earlier run had touched: Queens 821-822, Sudoku 353-354, Tango 661-662, Zip 500-501, Patches 135-136, Pinpoint 774-775 (puzzlegames.fun) and Wend 12-13, Crossclimb 771-772 (awa.games, because puzzlegames.fun has no unused IDs for them). 3 s between requests. The CLI got anonymised files (`case_001.txt`, ...) built only from what a player sees; Wend got the frozen 197k-word system list (not answer-derived). Published answers were read only afterwards by the independent Python checkers in the research scripts. Binary: Release build of exactly the 201-file public manifest. Search time = the CLI's own solve timer; wall time = whole process (dominated by start-up and, for word games, loading the dictionary).

| Game | Mode | Puzzle | Outcome | Rule-valid (Python) | Matches published | Nodes | Search ms | Wall ms |
|:--|:--|:-:|:--|:-:|:-:|:-:|:-:|:-:|
| queens | solve --solver bitset | 821 | UNIQUE | yes | n/a (source has no published answer) | 35 | 0.039 | 3.2 |
| queens | solve --solver mrv | 821 | UNIQUE | yes | n/a (source has no published answer) | 35 | 0.019 | 2.9 |
| queens | solve --solver baseline | 821 | UNIQUE | yes | n/a (source has no published answer) | 662 | 0.031 | 2.9 |
| queens | solve --solver bitset | 822 | UNIQUE | yes | n/a (source has no published answer) | 14 | 0.012 | 3.2 |
| queens | solve --solver mrv | 822 | UNIQUE | yes | n/a (source has no published answer) | 14 | 0.01 | 3.0 |
| queens | solve --solver baseline | 822 | UNIQUE | yes | n/a (source has no published answer) | 1133 | 0.045 | 2.9 |
| sudoku | solve | 353 | UNIQUE | yes | yes | 1 | 0.003 | 2.8 |
| sudoku | solve | 354 | UNIQUE | yes | yes | 1 | 0.004 | 2.8 |
| tango | solve | 661 | UNIQUE | yes | yes | 5 | 0.021 | 3.2 |
| tango | solve | 662 | UNIQUE | yes | yes | 7 | 0.028 | 3.1 |
| zip | solve | 500 | UNIQUE | yes | n/a (source has no published answer) | 67 | 0.079 | 3.0 |
| zip | solve | 501 | UNIQUE | yes | n/a (source has no published answer) | 180 | 0.21 | 3.0 |
| patches | solve | 135 | UNIQUE | yes | n/a (source has no published answer) | 11 | 0.017 | 3.1 |
| patches | solve | 136 | UNIQUE | yes | n/a (source has no published answer) | 11 | 0.008 | 2.9 |
| wend | solve --dict <197k system word list> | 12 | UNIQUE | yes | yes | 12 | 0.182 | 93.9 |
| wend | solve (built-in 1,253-word test list) | 12 | NO_SOLUTION | no | no | 2 | 0.024 | 4.3 |
| wend | solve --dict <197k system word list> | 13 | UNIQUE | yes | yes | 6 | 0.153 | 91.4 |
| wend | solve (built-in 1,253-word test list) | 13 | NO_SOLUTION | no | no | 1 | 0.02 | 4.1 |
| crossclimb | SUPPLIED ANSWER WORDS + endpoints | 771 | UNIQUE | yes | yes | 5 | 0.006 | 91.8 |
| crossclimb | SUPPLIED ANSWER WORDS, hidden endpoints, dictionary only | 771 | MULTIPLE | no | no | 10 | 0.114 | 90.9 |
| crossclimb | SUPPLIED ANSWER WORDS + endpoints | 772 | UNIQUE | yes | yes | 5 | 0.006 | 90.2 |
| crossclimb | SUPPLIED ANSWER WORDS, hidden endpoints, dictionary only | 772 | MULTIPLE | no | no | 5 | 0.159 | 90.3 |

Generators (new seed 20261110; no published answer, so only rule validity is checked):

| Game | Mode | Outcome | Rule-valid (Python) | Nodes | Search ms |
|:--|:--|:--|:-:|:-:|:-:|
| queens | generate --size 9 --seed 20261110 -> solve --check-unique | MULTIPLE | yes (both solutions checked) | 77 | 0.004 |
| sudoku | generate --seed 20261110 -> solve --check-unique | UNIQUE | yes | 1 | 0.003 |
| tango | generate --seed 20261110 -> solve --check-unique | UNIQUE | yes | 5 | 0.019 |
| zip | generate --rows 6 --cols 6 --checkpoints 6 --seed 20261110 -> solve --check-unique | UNIQUE | yes | 139 | 0.138 |

The Queens generator does not promise a unique puzzle: this 9×9 board has two valid solutions, both accepted by the Python checker. Sudoku and Tango generators promise uniqueness and delivered it here.

**Crossclimb, kept separate.** The two rows above labelled *SUPPLIED ANSWER WORDS* are given the five answer words (shuffled) by design; they test ordering, not clue solving. Supplied endpoints: 2/2 unique and equal to the published ladder. Hidden endpoints with only a dictionary: 2/2 ambiguous, as documented.

**Crossclimb from the clues alone (research prototype, not in the repo; `XC_BEAM=1`):**

| Puzzle | Decision | Middle answers in their own clue's candidate list (corrected 2026-11-10) | Published ladder in the solver's top 20 | Top-ranked ladder (valid, but wrong) | ms |
|:-:|:--|:-:|:-:|:--|:-:|
| 771 | ABSTAIN | 3/5 (SKIP rank 405, SPAR rank 896; list keeps 300) | no | SPAT SPAS SPAN SPIN SKIN SKIP SLIP | 2161 |
| 772 | ABSTAIN | 5/5 (correct middle path ranked 5th; lost at endpoints) | no | FIVE FINE LINE LIME TIME TAME LAME | 2031 |

Both abstained, which is the correct behaviour when unsure; neither was solved.

**Correction (2026-11-10).** The first version of the Crossclimb table above said 5/5 for both puzzles. That check counted a word found in *any* clue's list; the correct measure (the word in the list of the clue it answers) gives 3/5 for #771.

**Pinpoint, kept separate** (gold: 774 "Things that block sunlight", 775 'Different meanings of "run"'; judged by the frozen equivalence judge):

| Mode | Clues 1-5, puzzle 774 | Clues 1-5, puzzle 775 | Search ms (per guess) |
|:--|:--|:--|:-:|
| C++ guesser, shipped lexicon (fresh clues) | abstain / abstain / abstain / abstain / abstain | abstain / abstain / abstain / abstain / abstain | ≤ 11.1 |
| C++ guesser, ARCHIVE RETRIEVAL lexicon built from these puzzles' answers | correct / correct / correct / correct / correct | correct / correct / correct / correct / correct | ≤ 6.7 |
| open-category prototype (research/, macOS) | abstain / abstain / abstain / abstain / abstain | abstain / abstain / abstain / abstain / abstain | ≤ 8.7 |

Fresh-clue solving: the shipped C++ guesser abstained at every clue count (both categories are outside its 10-category lexicon); the open-category prototype also abstained (its top candidates, e.g. "Different definitions of 'blow'", were wrong). Archive retrieval is correct 10/10 only because its lexicon was built from these two puzzles' own answers and clues: it is lookup, not solving.

## 8. Crossclimb from clues: trace, change and second holdout (2026-11-10)

Prototype only (`research/scripts/crossclimb_clue/`, macOS, not in the public repo). Development examples: #771 and #772.

**Where the correct ladder dropped out.**
- #771 (STAR SPAR SPAN SPIN SKIN SKIP SHIP): **candidate generation.** SKIP ranks 405 and SPAR 896 for their clues; the list keeps 300. SKIP's "omit" sense is sense 3, past the 420-character excerpt the scorer reads. SPAR's boxing sense is a second dictionary homograph that the local dictionary API does not return, and the thesaurus only lists the "quarrel" sense, so no local evidence links SPAR to "boxer". Graph search and ranking never see the correct path.
- #772 (TAKE TAME TIME LIME LINE FINE FIVE): **endpoint ranking.** All five middle words are in their lists and the correct middle path is 5th of 25,711; the endpoint pair TAKE/FIVE got no phrase credit because "take five" has no headword of its own. It is listed in the PHRASES section of the "take" entry.
- Across the 100 development puzzles, keeping more candidates would raise the share of puzzles with all five answers in their lists (37 at 300, 73 at 1,500), but widening was tried and rejected: with 1,500 words per clue the one-letter graph has ~6,800 nodes and ~85,000 edges, both development puzzles hit the 20-million-expansion limit, and the correct path would still score below paths made of top-ranked words.

**Change (v2, `XC_VERSION=2`, `params_v2.json`; scoring weights unchanged).**
1. Endpoint phrase evidence also accepts a two-word phrase listed in the PHRASES section of either endpoint's dictionary entry (the measured #772 failure).
2. One-letter-transition pruning: drop (clue, word) nodes with no one-letter neighbour in another clue's list (repeated until stable), and require both ends of a middle chain to be one letter from some other dictionary word, since an endpoint must attach there.
3. Explicit limits: 300 candidates per clue, 5,000,000 search expansions, 60 s per puzzle; hitting a limit returns TIMEOUT, never a partial answer.

On the development examples v2 ranks #772's correct ladder first (it still abstains: a second ladder lies within the unchanged margin) and leaves #771 unchanged.

**Second frozen holdout.** Crossclimb IDs 721-770 from awa.games (50 puzzles, none used before; word length 4: 39, 5: 8, 6: 3). Inputs (word length, shuffled middle clues, combined clue) and gold were written to separate files and hashed (`holdout2_manifest.json`) before any run; the solver reads only `holdout2_inputs.json`, the scorer opens the gold after all solves.

| Holdout 2 (50) | Before (v1) | After (v2) |
|:--|:-:|:-:|
| Complete ladder solved (answered UNIQUE, correct) | 2 | 2 |
| Correct ladder listed among ≤3 alternatives | 1 | 2 |
| Wrong answer (answered, correct ladder not shown) | 1 | 1 |
| Abstained | 46 | 45 |
| Timeout | 0 | 0 |
| Correct ladder ranked first (answered or not) | 3 | 4 |
| Lost at candidate generation (an answer missing from its clue's list) | 38 | 38 |
| Lost at endpoints / ladder ranking | 6 | 6 |
| Ranked below first | 3 | 2 |
| Search expansions median / max / total | 385,097 / 598,297 / 13,598,579 | same |
| Seconds per puzzle median / max | 1.97 / 3.86 | 3.44 / 5.77 |

Exactly one puzzle changed (#721: ranked 3rd and abstained → ranked 1st, listed among alternatives); none got worse. The pruning removed only nodes that had no edges, so search work is identical; v2 is slower because of the PHRASES check. **The dominant loss is candidate generation (38/50)**, which this change does not touch; the next step is scoring every dictionary sense rather than the first 420 characters (it would rescue SKIP-type misses, not SPAR-type homograph gaps).

### 8.1 Controlled comparison of v1 and v2 (2026-11-10)

**Question.** Does v2 solve more Crossclimb puzzles from player-visible clues than v1 within the same search budget?

**Setup.** Both versions get identical treatment:
- **Inputs:** the same player-visible inputs (word length, the five middle clues shuffled with seed = puzzle id, and the combined top+bottom clue).
- **Resources:** the same word resources, loaded once per run.
- **Search:** the same candidate count (300 per clue), the same beam (`XC_BEAM=1`, B = 40) and the same number of middle paths sent to endpoint scoring (P = 100).
- **Budget:** the same end-to-end limit of 5,000,000 search expansions and 60 s per puzzle, covering candidate generation, search and endpoints. Exceeding either limit is a TIMEOUT. Before this comparison only v2 enforced limits, and its clock started after candidate generation. The solver now applies the limits to both versions, set by the evaluator. With no limits set, v1 behaves exactly as before.

The evaluator (`evaluate_controlled.py`) solves every puzzle before it opens the gold file. Three instrumentation changes were made to `clue_solver.py`. None of them changes scoring or the decision rule:
- the end-to-end clock and the limits shared by both versions;
- the solver now returns every middle path found, every node removed by pruning, and every scored ladder;
- these are used only by the scorer to locate the failure stage.

**Development cases (v1 → v2).**
- **#771 (STAR SPAR SPAN SPIN SKIN SKIP SHIP): v2 does not recover "skip" or "spar".**
  - Both versions share candidate generation and the 300 cutoff. Full-list ranks are identical in both: skip 405, spar 896, skin 21, and span and spin both 1.
  - Pruning removes 310 of 1,500 nodes but no answer, and both published endpoints are in the word list. The puzzle is lost at candidate generation either way: 7,357 v1 vs 7,356 v2 middle paths, 91,429 expansions each, 2.2 s → 3.6 s.
- **#772 (TAKE TAME TIME LIME LINE FINE FIVE): the correct middle path survives.**
  - The path survives in both versions: rank 5 of 30,382 complete paths, 313,800 expansions each.
  - Under v1 the published endpoints TAKE/FIVE rank 5th among the endpoint pairs on that path, and the full ladder ranks 87th.
  - Under v2 the "take five" PHRASES evidence lifts them to 1st on that path and 1st overall. The decision rule still abstains, because another ladder lies within its unchanged margin. Time: 2.0 s → 4.6 s.

**Holdout 3: frozen before any solver run on these IDs and before any tuning.**
- **Source:** Crossclimb IDs 621-720 from awa.games, fetched 3 s apart. None of these IDs appears in any earlier run (development 823-922, holdout 774-822, dev examples 771-772, holdout 2 721-770).
- **Freezing:** `freeze_holdout.py` writes the player-visible inputs and the scorer-only gold to separate files. `holdout3_manifest.json` records sha256 hashes of both files and of the solver, parameter files, evaluator and freeze script. These hashes were rechecked immediately before the runs. The same script rebuilds holdout 2 byte-for-byte.
- **Actual size: 99 puzzles** (word length 4: 71, 5: 24, 6: 4).
- **Coverage gaps:**
  - #701 is a number ladder (digits, not words). It is outside the word solver's scope and excluded.
  - #622 was first stored empty by an interrupted parser test. It was refetched and the holdout re-frozen before any solver run.
  - Resource gaps that cap both versions: 4 middle answers and 2 published endpoints are not in the local word list at all.

| Holdout 3 (99 puzzles) | v1 | v2 |
|:--|:-:|:-:|
| **Exact full ladder solved** (answered UNIQUE, correct) | **3** | **3** |
| Correct ladder listed among ≤3 alternatives | 2 | 2 |
| Wrong answer | 2 | 2 |
| Abstained | 92 | 92 |
| Timeout | 0 | 0 |
| Correct ladder ranked first (answered or not) | 11 | 10 |
| *Failure stage:* missing clue candidate | 61 | 61 |
| *Failure stage:* search pruning (transition prune, endpoint check or beam) | 0 | 0 |
| *Failure stage:* middle-path ranking (path found, ranked below P = 100) | 7 | 7 |
| *Failure stage:* endpoint ranking (path got endpoints, ladder not first) | 20 | 21 |
| *Failure stage:* decision rule (ranked first, not answered) | 8 | 7 |
| Graph nodes, median (after pruning) | 1,500 | 1,332 |
| Graph edges, median | 4,140 | 4,140 |
| Search expansions: median / p90 / max | 428,511 / 569,837 / 2,840,363 | identical |
| Search expansions, total | 37,934,125 | identical |
| End-to-end seconds per puzzle: median / p90 / max | 2.20 / 3.09 / 4.41 | 3.62 / 5.59 / 7.41 |
| Total solve time (99 puzzles) | 228 s | 381 s |

**Per-puzzle changes.** No answered outcome changed. Eight puzzles moved, all for the worse:
- **#660:** the published ladder was ranked first under v1 and is ranked 2nd under v2.
- **Seven others:** the published ladder dropped between 1 and 24 places.

The cause is the v2 PHRASES rule. It credits any two words that appear next to each other anywhere in a PHRASES section, including inside longer phrases and example sentences. That gives wrong pairs the same phrase bonus as genuine ones. In #660, "look / done" gets the bonus and outranks the published "gene pool".

**Missing clue candidates (61 puzzles, 90 missing answers).**
- **Not in the word list at all:** 4 answers.
- **Ranked below the 300 cutoff:** 86 answers. 33 rank 301-600, 20 rank 601-1,500 and 33 rank beyond 1,500 (median 917).
- **Missing answers per puzzle:** 38 puzzles miss none, 39 miss one, 16 miss two, 5 miss three and 1 misses four.

**Same budget rerun on holdout 2.** Holdout 2 is not fresh: it scored v2 earlier. The result is consistent with holdout 3:
- solved 2 → 2, listed 1 → 2, wrong 1 → 1, abstained 46 → 45, timeouts 0 → 0;
- ranked first 3 → 4: #721 is the one gain, and two ranks dropped slightly;
- identical expansions (median 385,097);
- median time 2.23 s → 3.93 s.

Over the two holdouts combined (149 puzzles), v2 solves the same number of complete ladders (5) and ranks one fewer correct ladder first (15 → 14).

**Conclusion.**
- Within the same budget, v2 does not solve more puzzles.
- Its transition pruning removes about 11% of nodes but none of the edges the search uses. Search work is therefore unchanged and the pruning never removed a correct answer.
- The PHRASES rule fixes the case it was built for (#772, #721) but adds false phrase credit elsewhere.
- v2 costs about 65% more time end to end.
- Neither version ever reached a limit. The largest search used 2.84 M of the 5 M expansions, and the slowest puzzle took 7.4 s of the 60 s.

**Recommendation: v2 is not ready for the public export.**
- Keep v1 as the prototype default. Leave v2 opt-in (`XC_VERSION=2`), documented as a negative result.
- The prototype as a whole stays in git-ignored `research/` in any case. It depends on macOS dictionary and embedding resources and solves 3 of 99 fresh puzzles, so the public export should continue to describe Crossclimb from clues as unsupported. Only supplied-endpoint mode is supported.
- **Next lever:** candidate generation, which is the first loss in 61 of 99 puzzles. Promising directions are scoring every dictionary sense rather than the first 420 characters, and a better clue-word score; a wider cutoff alone is not enough (see §8). After that, the PHRASES rule would need to require the pair to be a complete phrase (both words, nothing between them, at a phrase boundary) before any re-evaluation. Any re-evaluation needs a new frozen holdout, because holdouts 2 and 3 have now been used to compare v1 and v2.

**Reproduce** (macOS; run from `research/scripts/crossclimb_clue/`, with `PINPOINT_OPEN_SCRATCH` pointing at the local resource cache):

```bash
python3 freeze_holdout.py holdout3 holdout3_raw   # only to rebuild; the frozen files and manifest are already in fixtures/
XC_VERSION=1 python3 evaluate_controlled.py holdout3 ../../fixtures/crossclimb_clue/holdout3_results_v1.json
XC_VERSION=2 python3 evaluate_controlled.py holdout3 ../../fixtures/crossclimb_clue/holdout3_results_v2.json
python3 compare_versions.py ../../fixtures/crossclimb_clue/holdout3_results_v1.json ../../fixtures/crossclimb_clue/holdout3_results_v2.json
```

`evaluate_controlled.py dev` and `evaluate_controlled.py holdout2` run the development trace and the holdout 2 rerun. Their saved outputs are `dev_controlled_v{1,2}.json` and `holdout2_controlled_v{1,2}.json`.

### 8.2 Systematic debugging of the v2 phrase rule, and holdout 4 (2026-11-10)

Method: the Superpowers *systematic-debugging* skill (plugin v7.0.0 from the official Claude Code marketplace, installed at local scope in this working copy only). The skill requires root cause, then pattern, then a single hypothesis, then a failing test, then one fix, then verification.

**Defect.** v2 pushed correct ladders down on holdout 3 (§8.1). For example, #660 dropped from rank 1 to rank 2.

**Phase 1: root cause.**
- `phrases_of` returns an entry's PHRASES section as one flattened string: "entry definition: example. entry definition: example. …".
- The v2 rule accepted the two endpoint words wherever they appeared next to each other in that string.
- Reproduced on the three wrong pairs from holdout 3:
  - "look / done" matched inside an example sentence ("you look done in");
  - "have / part" matched across an entry/definition boundary ("look the part | have an appearance…");
  - "love / made" matched inside an example sentence ("they made love").
- The genuine "take five" sits at the start of an entry ("…sit down. take five informal…").

**Phases 2–3: pattern and hypothesis.** A phrase entry begins at the start of the section or right after ". ", while examples follow ": ". Hypothesis: crediting the pair only at an entry start removes the false credit and keeps genuine phrases.

**Phase 4: test, fix, verify.**
- **Failing test first.** `test_phrase_rule.py` failed 2/5 under v2.
- **Fix 1 (entry start only).** The test passed 5/5. On the development set (100 puzzles plus #771/#772), the wrong top-20 pairs credited by the rule fell from 64 to 20 of 1,245. However, the rule now credited only 1 of the 5 published pairs, down from 5.
- **Back to Phase 1.** Three of the lost pairs are real entries that open with a leading word: "a free hand", "a clean sweep", "be good news". The fourth, "deal / hand", was an accidental boundary-crossing match all along.
- **Fix 2 (same root cause, corrected entry boundary).** An entry may open with "a", "an", "the" or "be". The four cases were added to the test first and failed. With the fix, the test passes 9/9:
  - the rule credits 4 of 5 published pairs on the development set (the lost one is the accidental match);
  - it credits 26 of 1,245 wrong pairs, down from 64. Most of the remaining 26 are real phrases that aren't that puzzle's answer, such as "fine line", "give rise" and "save face".
- **Stopped at two attempts**, as the skill requires.

The fix is `XC_VERSION=2.1`. It differs from v2 only in `phrase_in_sections`; `XC_VERSION=2` still reproduces §8.1.

**Holdout 4.** Crossclimb IDs 521-620, never used before, fetched 3 s apart.
- **Freezing:** the holdout was frozen after fix 1 and before any solver run on it (`holdout4_manifest.json`). Fix 2 came after the freeze but used development puzzles only. The manifest records this, with the file hashes used for the runs.
- **Size: 99 puzzles** (word length 4: 63, 5: 29, 6: 7).
- **Gaps:** #537 has no clues on the source page. As before, 4-letter puzzles can form dense candidate graphs.
- **Budget:** identical for every version, as in §8.1 (5 M expansions, 60 s, beam, 300 candidates).

| Holdout 4 (99 puzzles) | v1 | v2 | v2.1 |
|:--|:-:|:-:|:-:|
| **Exact full ladder solved** | **3** | **3** | **3** |
| Correct ladder listed among ≤3 alternatives | 2 | 2 | 2 |
| Wrong answer | 1 | 2 | 1 |
| Abstained | 92 | 91 | 92 |
| Timeout | 1 | 1 | 1 |
| Correct ladder ranked first (answered or not) | 11 | 10 | 11 |
| All five answers in their clue's list (candidate recall) | 27 | 27 | 27 |
| *Failure stage:* missing clue candidate | 71 | 71 | 71 |
| *Failure stage:* search pruning | 0 | 0 | 0 |
| *Failure stage:* middle-path ranking | 3 | 3 | 3 |
| *Failure stage:* endpoint ranking | 13 | 14 | 13 |
| *Failure stage:* decision rule | 8 | 7 | 8 |
| Graph nodes, median | 1,500 | 1,226 | 1,226 |
| Search expansions: median / p90 / total | 100,084 / 525,179 / 28.50 M | same | same |
| End-to-end seconds: median / p90 / max | 2.19 / 2.93 / 5.68 | 3.73 / 5.43 / 6.65 | 3.48 / 5.26 / 7.07 |
| Total solve time (99 puzzles) | 219 s | 362 s | 347 s |

- **The timeout is #607 in all three versions.** Its short words (ayes / eyes / odes / eves) form a candidate graph with 23,086 links, and the search hits the 5 M-expansion limit in under 4 s. The run reports TIMEOUT rather than a partial answer. One answer ("ades") is also missing from its clue's list, so the puzzle was not solvable within the candidate list anyway.
- **v2's regressions reappear on fresh data:** #531 drops from rank 1 to 2, #532 becomes a wrong answer, and six ranks drop. v2.1 removes all of them except small rank losses on #569 and #574, and matches v1 on every outcome.
- **v2.1 fixes the defect but adds no solves.** Development #772's correct ladder is still ranked first under v2.1 and still abstains at the decision rule.

**Conclusion.**
- The v2 regression's root cause is found and fixed: v2.1 is back at v1's level.
- Phrase-section evidence does not add solves on 99 fresh puzzles, and it costs about 60% more time.
- Across holdouts 3 and 4 (198 puzzles), candidate generation is the first loss for 132.

**Recommendation (unchanged).** Keep v1 as the prototype default, with v2 and v2.1 opt-in. Don't put the clue solver in the public export. The next investigation should target candidate generation (§8.1), evaluated on a new frozen holdout: IDs below 521 are unused. Holdouts 2, 3 and 4 are now spent for this question.

**Reproduce** (from `research/scripts/crossclimb_clue/`, `PINPOINT_OPEN_SCRATCH` set):

```bash
XC_VERSION=2.1 python3 test_phrase_rule.py
for v in 1 2 2.1; do XC_VERSION=$v python3 evaluate_controlled.py holdout4 ../../fixtures/crossclimb_clue/holdout4_results_v$v.json; done
```

## 9. Explainable solvers and the clue experiments (2026-11-10)

**Shipped.** All eight games have `hint`, `deduce` and `explain` (`ALGORITHMS.md` §11).
- **Before-baseline:** `research/fixtures/baseline_before.json`, the same 373 boards as `research/scripts/grid_baseline.py`. Afterwards, the original `solve` commands give identical statuses and node counts on all of them, apart from four 10 s timeouts.
- **After-results:** `research/fixtures/reasoning_<game>.json` (`research/scripts/reasoning_bench.py`), on the same boards, budget, release build and machine.
- **Correctness checks:** every new rule has a regression test with a near-miss where it must do nothing (`tests/test_*_logic.cpp`). The original solvers act as oracles: on 157 Queens, 263 Sudoku, 202 Tango, 141 Zip, 26 Patches, 22 Wend and 23 Crossclimb boards, every forced step must agree with the unique solution, or must keep a known solution's queens.

**Crossclimb clue experiment v3 (research only, not shipped).** The diagnosis comes first:
- **Holdout 3:** 86 of 90 missing answers are in the vocabulary.
- **Holdout 5:** 88 of the 101 answers outside the top 300 are in the vocabulary; 13 are not.
- **Conclusion:** the loss is retrieval and ranking, not vocabulary.

The one targeted change was to score each dictionary sense separately (`XC_VERSION=3`; weights, search and endpoints unchanged). Two variants were tried on development puzzles 823–922 only:
- **Pure best sense:** answers ranked first fell from 146 to 128 of 500, and top-300 recall from 408 to 405.
- **Best of original and best sense:** top-50 recall rose from 332 to 343, but top-300 fell from 408 to 406, and 218 answers moved down against 75 that moved up.

Holdout 5 (IDs 421–520, 99 puzzles) was frozen before any run on it (`holdout5_manifest.json`; #446 excluded: its page does not have 5 clues and 7 words) and scored under the same budget as §8.1:

| Holdout 5 (99 puzzles) | v1 | v3 |
|:--|:-:|:-:|
| Answers in their clue's top 300 (of 495) | 394 | 395 |
| Puzzles with all five in the top 300 | 29 | 29 |
| Exact full ladder solved | 3 | 3 |
| Correct ladder listed among alternatives | 1 | 1 |
| Wrong answer | 4 | 6 |
| Abstained | 91 | 89 |
| Timeout | 0 | 0 |
| Lost at: missing clue candidate | 70 | 70 |
| Lost at: endpoint ranking | 21 | 21 |
| Search expansions, median | 89,398 | 93,087 |
| Seconds per puzzle, median | 2.32 | 2.33 |

**Verdict.** v3 does not improve candidate recall, and it adds two wrong answers. v1 stays the default, and endpoint ranking was not tuned.

**Pinpoint `explain`** (shipped), evaluated with each run seeing only the first k clues:
- **Policy fixed on development puzzles 823–922:** compound hypotheses are display-only, because 26 of 26 single-affix answers were wrong.
- **Frozen unseen set (46 puzzles, frozen judge):** 0 answered and 0 wrong at every k = 1..5. The correct before/after hypothesis is in the top 3 shown for 1 puzzle.

**Reproduce:**

```bash
python3 research/scripts/grid_baseline.py research/fixtures/baseline_before.json
python3 research/scripts/reasoning_bench.py research/fixtures/baseline_before.json research/fixtures/reasoning_tango.json tango
python3 research/scripts/wend_eval.py research/fixtures/reasoning_wend.json research/fixtures/wend_lexicon/baseline_plus_ode_v1.txt
(cd research/scripts/crossclimb_clue && XC_VERSION=3 python3 candidate_recall.py holdout5 ../../fixtures/crossclimb_clue/holdout5_recall_v3.json)
(cd research/scripts/crossclimb_clue && XC_VERSION=3 python3 evaluate_controlled.py holdout5 ../../fixtures/crossclimb_clue/holdout5_results_v3.json)
python3 research/scripts/pinpoint_explain_eval.py holdout research/fixtures/external_crosscheck/frozen/web2_upper_2to12.txt
```
