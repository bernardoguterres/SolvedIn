# SolvedIn

> **Unofficial; not affiliated with or endorsed by LinkedIn.** "LinkedIn" appears here only to describe which puzzles the rules are modelled on. Read [`LIMITATIONS.md`](LIMITATIONS.md) before relying on any result.

SolvedIn is an offline solver, generator, and validator for all eight LinkedIn puzzle games (**Queens**, **Mini Sudoku**, **Tango**, **Zip**, **Patches**, **Wend**, **Crossclimb**, and **Pinpoint**), written in **C++20** using **CMake** and **CTest**.

---

## Overview of Games & Implementations

### 1. Queens
- **Seeded Queens Generator**: `solvedin queens generate --size N --seed S [--output <file>]`. Plants one valid solution but does **not** guarantee uniqueness (check with `--check-unique`).
- **Three Solver Variants**:
  - `baseline`: Sequential row-by-row backtracking.
  - `mrv`: Minimum Remaining Values heuristic (branches on the row, column, or region with fewest legal cells).
  - `bitset`: Multi-word bitsets with hardware-accelerated popcount and CTZ (`std::countr_zero`), with forward checking (`bitset_has_wipeout`) and least-constraining-value cell sorting.
- **Giant Queens Evaluation Suite**: Benchmark evaluating boards from 8×8 to 128×128 with memory profiling (`getrusage`), budget exhaustion handling (`INCONCLUSIVE`), and high-resolution SVG export (`giant_queens_128x128.svg`).
- **Large boards**: on the macOS-generated boards, seed 42 solves 64×64 in 2.02 ms (65 nodes) and 128×128 in ≈ 50 ms (737 nodes). Generated boards differ per platform (see below), and on Linux the 128×128 seed-42 board exhausts its node budget.

### 2. Mini Sudoku (6×6)
- **Grid Structure**: 6×6 grid with six 2-row by 3-column ($2 \times 3$) boxes.
- **Rules**: Digits 1 through 6. Each row, column, and box must contain each digit 1..6 exactly once while preserving all initial givens.
- **Solving Engine**: 6-bit candidate masks per cell, constraint propagation with forced moves (naked singles and hidden singles across rows, columns, and boxes), and MRV backtracking.
- **Seeded Generator**: `solvedin sudoku generate --seed S [--remove N] [--output <file>]`. Guarantees a unique solution.
- **SVG Exporter**: Responsive SVG vector graphic (`--export-svg <file>`) with 2×3 box boundaries, styled givens, and solved digit pills.
- **Independent Validator**: Independently checks all row, column, and box constraints and verifies given preservation.

### 3. Tango (Sun & Moon with Edge Clues)
- **Grid Structure**: $N \times N$ grid (typically 6×6) with symbols Sun (`S` / `O`) and Moon (`M` / `X`).
- **Rules**:
  1. Equal counts: Exactly $N/2$ Sun and $N/2$ Moon per row and per column (for 6×6: 3 of each).
  2. No three consecutive: No run of three identical symbols horizontally or vertically (`SSS` and `MMM` forbidden).
  3. Horizontal and vertical edge clues between adjacent cells:
     - Same clue (`=`): both adjacent cells must contain the same symbol ($S = S$ or $M = M$).
     - Opposite clue (`x`): adjacent cells must contain opposite symbols ($S \times M$ or $M \times S$).
  4. Preserves all initial givens.
- **Solving Engine**: 2-bit domain masks per cell, edge-clue constraint propagation, count-limit pruning, 3-in-a-row lookahead, and backtracking.
- **Seeded Generator**: `solvedin tango generate --seed S [--givens G] [--clues C] [--output <file>]`. Guarantees a unique solution.
- **SVG Exporter**: Responsive SVG with crisp vector Sun and Moon glyphs and `=` / `x` edge clue badges (`--export-svg <file>`).
- **Independent Validator**: Verifies all rules and edge clue constraints independently.

### 4. Zip (Continuous Path with Checkpoints and Walls)
- **Grid Structure**: $R \times C$ grid with numbered checkpoints $1 \dots K$ and horizontal/vertical walls.
- **Rules**:
  1. Hamiltonian path: Draw a single continuous orthogonal path that visits every cell on the board exactly once.
  2. Sequential checkpoints: Path starts at checkpoint 1 and ends at checkpoint $K$, visiting checkpoints $1, 2, \dots, K$ in strictly ascending numerical order.
  3. Obey walls: Movement cannot cross horizontal walls (`WALLS_H`) or vertical walls (`WALLS_V`).
- **Solving Engine**:
  - Head-first DFS with Warnsdorff-style degree heuristic.
  - **Cut-Off Pruning**: At each search step, an unvisited-cell BFS verifies that all unvisited cells are reachable from the path head. If any unvisited cell is cut off, or if the next required checkpoint is unreachable, the branch is immediately pruned.
  - **Dead-End / Degree Pruning**: Unvisited intermediate cells with degree $< 2$ (or final destination with degree $< 1$) trigger immediate pruning.
- **Seeded Generator**: `solvedin zip generate --rows R --cols C --checkpoints K --seed S [--output <file>]`.
- **SVG Exporter**: High-resolution vector path with numbered checkpoint circles, wall lines, and directional arrows (`--export-svg <file>`).
- **Independent Validator**: Checks path length, cell coordinates, exact cell coverage, orthogonal continuity, wall crossings, and strictly ascending checkpoint order.

### 5. Patches (Exact Board Tiling with Clues)
- **Grid Structure**: $R \times C$ board with clue cells.
- **Rules**:
  1. Exact partition: In LinkedIn Patches, the board is partitioned into non-overlapping **solid rectangles and squares** such that every cell belongs to exactly one rectangle. Non-rectangular polyominoes are not permitted.
  2. Exactly one clue per rectangle.
  3. Shape constraints:
     - **Square (`S`)**: Solid rectangle with width $w == height\ h$ ($1\times 1, 2\times 2, 3\times 3, \dots$).
     - **Tall rectangle (`T`)**: Solid rectangle with height $h > width\ w$ ($2\times 1, 3\times 1, 3\times 2, \dots$).
     - **Wide rectangle (`W`)**: Solid rectangle with width $w > height\ h$ ($1\times 2, 1\times 3, 2\times 3, \dots$).
     - **Freeform (`F`)**: Any rectangular aspect ratio ($w == h$, $h > w$, or $w > h$), enforcing the specified area if given.
  4. Clue types supported: Size only (`4`), Shape only (`S`, `T`, `W`, `F`), Both (`4S`, `6W`, `4T`, `4F`), or Neither (`?`).
- **Solving Engine**: Factor-based rectangular candidate enumeration for each clue anchor; 128-bit bitmask exact cover search with MRV backtracking and pre-branch clue wipeout detection.
- **SVG Exporter**: Distinct color-tinted patch tiles with borders and clue badges (`--export-svg <file>`).
- **Independent Validator**: Verifies the solid rectangle invariant ($|cells| == height \times width$), complete non-overlapping grid coverage, single-clue containment, and shape/size compliance. Rejects non-rectangular covers (e.g. L-trominoes).

### 6. Wend (Orthogonal Trie-Based Word Cover)
- **Grid Structure**: $R \times C$ grid of uppercase letters with specified target word lengths (e.g. `LENGTHS: 3 3 3`).
- **Rules**: Traced orthogonal non-self-intersecting paths covering every cell once, matching target lengths, and valid in dictionary.
- **Solving Engine**: Fast 26-way array `Trie` prefix matching; preserves distinct geometric paths spelling identical words; cell-driven MRV exact cover.
- **SVG Exporter**: Letter grid with colored snake paths showing each word cover and word legend (`--export-svg <file>`).
- **Independent Validator**: Validates orthogonal continuity, bounds, dictionary membership, multiset length match, and exact non-overlapping partition.

### 7. Crossclimb (Word Ladder Graph with Missing Endpoints)
- **Grid Structure**: Word ladder puzzle of length $W$ with middle rung words, start word (or `?`), and end word (or `?`).
- **Rules**: Consecutive words differ by 1 letter (Hamming distance 1), sequences all rungs, resolves missing endpoints, detects ambiguity (`MULTIPLE_SOLUTIONS`).
- **Solving Engine**: Hamming-1 word graph, 1-letter candidate generation from Trie, backtracking Hamiltonian path search.
- **SVG Exporter**: Word ladder vertical graphic with rails, rungs, and highlighted 1-letter transitions (`--export-svg <file>`).
- **Independent Validator**: Checks ladder length, word uniqueness, Hamming-1 steps, permutation of rungs, and dictionary membership.

### 8. Pinpoint (Offline Ranked Category Guesser)
- **Game Concept**: Given a set of up to 5 progressive clues, guess the common theme/category.
- **Features**: Editable text category lexicon (`fixtures/pinpoint_lexicon.txt`), clue attribution (supported vs. unsupported), confidence ranking, weak evidence abstention (`STATUS: UNKNOWN`).

---

## Build & Test Instructions

### Release Build:
```bash
cmake -B build_release -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build_release -j8
```

### Run CTest Suite (17 Tests):
```bash
ctest --test-dir build_release --output-on-failure
```

### Run Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer):
```bash
cmake -B build_asan -S . -DENABLE_SANITIZERS=ON
cmake --build build_asan -j8
ctest --test-dir build_asan --output-on-failure
```

---

## File Formats

### 1. Queens
```text
4
1 1 2 2
1 1 2 2
3 3 4 4
3 3 4 4
```

### 2. Mini Sudoku
```text
6
. 2 3 . . .
4 5 . . . .
. 3 . 5 . 4
5 . . . . 1
. . . . . 5
. . . 3 1 .
```

### 3. Tango
```text
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
. . . . .
. x . . .
VERTICAL
. . . . . .
. . . . x .
. . . . . .
. . . = x .
. . . . . .
```

### 4. Zip
```text
4 4
GRID
1 . . 2
. . . .
3 . . .
5 . . 4
WALLS_H
- - - .
. - - -
- - - .
```

### 5. Patches
```text
6 6
GRID
4S  .  6W   .   .  4T
 .  .   .   .   .   .
5F  .   .   4   .   .
 .  .   ?   .   .   .
4S  .   T  6W   .   .
 .  .   .   .   .   .
```

### 6. Wend
```text
3 3
GRID
F O R
V A N
P I G
LENGTHS: 3 3 3
```

### 7. Crossclimb
```text
WORD_LENGTH: 4
START: COLD
RUNGS:
CARD
WARD
CORD
END: WARN
```

### 8. Pinpoint (Clues File & Lexicon)
Clues File (`fixtures/pinpoint_clues_drives.txt`):
```text
CLUES:
FLOPPY
HARD
COMPACT
FLASH
SOLID STATE
```

Category Lexicon (`fixtures/pinpoint_lexicon.txt`):
```text
CATEGORY: Computer Storage Drives
TERMS: FLOPPY, HARD, COMPACT, FLASH, SOLID STATE, ZIP, THUMB, USB, OPTICAL, CLOUD
DESCRIPTION: Types of computer storage media and drives
```

---

## Working CLI Examples & Actual Output

### 1. Queens Solve & SVG Export
```bash
./build_release/solvedin queens solve fixtures/valid_8x8_unique.txt --check-unique --export-svg /tmp/queens.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/queens.svg
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
Solver variant: baseline
Nodes visited : 168
Elapsed time  : 0.010 ms
Budget status : WITHIN_BUDGET
```

### 2. Mini Sudoku Generate & Solve with SVG Export
```bash
./build_release/solvedin sudoku generate --seed 42 --remove 20 --output /tmp/sudoku.txt
./build_release/solvedin sudoku solve /tmp/sudoku.txt --check-unique --export-svg /tmp/sudoku.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/sudoku.svg
Unique solution found:
1 2 6 | 5 3 4
5 4 3 | 6 2 1
------+-------
3 1 2 | 4 6 5
4 6 5 | 3 1 2
------+-------
2 3 4 | 1 5 6
6 5 1 | 2 4 3

--- Search Metrics ---
Nodes visited : 1
Elapsed time  : 0.003 ms
Budget status : WITHIN_BUDGET
```

### 3. Tango Generate & Solve with SVG Export
```bash
./build_release/solvedin tango generate --seed 101 --givens 8 --clues 6 --output /tmp/tango.txt
./build_release/solvedin tango solve /tmp/tango.txt --check-unique --export-svg /tmp/tango.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/tango.svg
Unique solution found:
M S M S S M
S M S S M M
M S M M S S
S M S S M M
S M S M M S
M S M M S S

--- Search Metrics ---
Nodes visited : 4
Elapsed time  : 0.040 ms
Budget status : WITHIN_BUDGET
```

### 4. Zip Generate & Solve with SVG Export
```bash
./build_release/solvedin zip generate --rows 4 --cols 4 --checkpoints 4 --seed 1001 --output /tmp/zip.txt
./build_release/solvedin zip solve /tmp/zip.txt --check-unique --export-svg /tmp/zip.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/zip.svg
Unique solution found:
--- Path Step Grid ---
 4  5  6  7
 3 14 15  8
 2 13 16  9
 1 12 11 10
Path (16 steps):
  (3, 0) -> (2, 0) -> (1, 0) -> (0, 0) -> (0, 1) -> (0, 2) -> 
  (0, 3) -> (1, 3) -> (2, 3) -> (3, 3) -> (3, 2) -> (3, 1) -> 
  (2, 1) -> (1, 1) -> (1, 2) -> (2, 2)

--- Search Metrics ---
Nodes visited : 16
Elapsed time  : 0.021 ms
Budget status : WITHIN_BUDGET
```

### 5. Patches Solve with SVG Export
```bash
./build_release/solvedin patches solve fixtures/patches_valid_unique.txt --check-unique --export-svg /tmp/patches.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/patches.svg
Unique solution found:
--- Patch Tiling Grid ---
A A B B B C
A A B B B C
H H H I I C
H H D I I C
G G E F F F
G G E F F F
Patches (9 total):
  Patch A [clue '4S' at (0, 0), size 4]: (0,0) (0,1) (1,0) (1,1)
  Patch B [clue '6W' at (0, 2), size 6]: (0,2) (0,3) (0,4) (1,2) (1,3) (1,4)
  Patch C [clue '4T' at (0, 5), size 4]: (0,5) (1,5) (2,5) (3,5)
  Patch D [clue '?' at (3, 2), size 1]: (3,2)
  Patch E [clue 'T' at (4, 2), size 2]: (4,2) (5,2)
  Patch F [clue '6W' at (4, 3), size 6]: (4,3) (4,4) (4,5) (5,3) (5,4) (5,5)
  Patch G [clue '4S' at (4, 0), size 4]: (4,0) (4,1) (5,0) (5,1)
  Patch H [clue '5F' at (2, 0), size 5]: (2,0) (2,1) (2,2) (3,0) (3,1)
  Patch I [clue '4' at (2, 3), size 4]: (2,3) (2,4) (3,3) (3,4)

--- Search Metrics ---
Nodes visited : 11
Elapsed time  : 0.081 ms
Budget status : WITHIN_BUDGET
```

### 6. Wend Solve with SVG Export
```bash
./build_release/solvedin wend solve fixtures/wend_valid_unique.txt --check-unique --export-svg /tmp/wend.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/wend.svg
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
Elapsed time  : 0.007 ms
Budget status : WITHIN_BUDGET
```

### 7. Crossclimb Solve with SVG Export
```bash
./build_release/solvedin crossclimb solve fixtures/crossclimb_valid_unique.txt --export-svg /tmp/crossclimb.svg --stats
```
```text
Exported solution visualization to SVG: /tmp/crossclimb.svg
Unique solution found:
Crossclimb Ladder (5 rungs):
  [0] COLD (START)
  [1] CORD
  [2] CARD
  [3] WARD
  [4] WARN (END)

--- Search Metrics ---
Nodes visited : 3
Elapsed time  : 0.004 ms
Budget status : WITHIN_BUDGET
```

### 8. Pinpoint
```bash
./build_release/solvedin pinpoint guess fixtures/pinpoint_clues_drives.txt
```
```text
Pinpoint Guesser Result (5 clues provided):
Clues: [FLOPPY, HARD, COMPACT, FLASH, SOLID STATE]

STATUS: CONFIDENT GUESS
  #1 Computer Storage Drives (confidence: 100.0%)
     Supported (5): FLOPPY HARD COMPACT FLASH SOLID STATE 
```

---

## Giant Queens Benchmark Summary

The Giant Queens evaluation suite (`benchmarks/giant_queens.cpp`) benchmarks deterministic seeded boards from 8×8 up to 128×128, separating time to first valid solution from uniqueness verification and budget exhaustion.

### Results Table:
- **Largest Board Solved**: **128×128** (Seed 42, 16,384 cells)
- **First Solution Phase**: **56.41 ms** (737 search nodes)
- **Uniqueness Verification Phase**: **56.98 ms** (739 search nodes)
- **Status Returned**: `MULTIPLE_SOLUTIONS` (Uniqueness **disproven**; second valid queen placement found)
- **Peak Memory**: **74.11 MB** (RSS)
- **Solver Variant**: `bitset` (multi-word bitsets with hardware popcount, CTZ, pre-branch forward checking & LCV)
- **Exported SVG**: `giant_queens_128x128.svg`
- **64×64 Performance**:
  - Seed 42: First solution in **1.72 ms** (65 nodes); Uniqueness check in **1.67 ms** (67 nodes, `MULTIPLE_SOLUTIONS`)
  - Seed 7: First solution in **2.13 ms** (65 nodes); Uniqueness check in **2.23 ms** (67 nodes, `MULTIPLE_SOLUTIONS`)
  - Seed 1: First solution in **3.20 ms** (114 nodes); Uniqueness check in **3.01 ms** (117 nodes, `MULTIPLE_SOLUTIONS`)
- **Reproduced Budget Exhaustion Instances**:
  - Seed 7 on 128×128: Reached 79,360 nodes (3,021 ms), returning **`INCONCLUSIVE`**
  - Seed 42 on 32×32: Reached 200,000 nodes (155 ms), returning **`INCONCLUSIVE`**

### Reproduction Commands:
```bash
# 1. Generate 128x128 board with recorded seed 42
./build_release/solvedin queens generate --size 128 --seed 42 --output board_128x128.txt

# 2. Solve and export SVG visualization
./build_release/solvedin queens solve board_128x128.txt --solver bitset --export-svg giant_queens_128x128.svg --stats

# 3. Run full Giant Queens benchmark runner
./build_release/giant-queens
```

---

## Multi-Tier Corpus Benchmark (All 8 Games)

The multi-tier corpus benchmark runner (`benchmarks/corpus_benchmark.cpp`) evaluates 72 locally generated fixtures (easy for a modern machine; they do not predict performance on real puzzles) across all eight games in three difficulty tiers (Easy, Medium, Hard).

### Corpus Tier Construction Methodology:
- **Queens**:
  - *Easy*: 8×8, 10×10 boards with planted non-touching queens and connected region partitions.
  - *Medium*: 16×16, 20×20, 24×24 boards with larger region boundaries.
  - *Hard*: 32×32, 64×64, 128×128 giant boards exercising multi-word bitsets (up to 16,384 cells).
- **Mini Sudoku**:
  - *Easy*: 16 cell removals (20 givens remaining); solved via naked singles alone.
  - *Medium*: 20 cell removals (16 givens remaining); solved via naked and hidden singles across rows, columns, and boxes.
  - *Hard*: 24 cell removals (12 givens remaining); propagation stalls, demonstrably requiring MRV depth-first branching and backtracking (`fixtures/sudoku_branching.txt`, 4 search nodes).
- **Tango**:
  - *Easy*: 12 givens (33% filled) and 6 edge clues (`=`, `x`); directly reducible with 1 node.
  - *Medium*: 8 givens and 5 edge clues; requires 2–4 search nodes.
  - *Hard*: 4 givens (11% filled) and 4 edge clues; requires deeper backtracking (up to 42 nodes).
- **Zip**:
  - *Easy*: 4×4 grids with 3 checkpoints and boundary walls (24–28 nodes).
  - *Medium*: 5×5 grids with 4 checkpoints and interior wall segments (39–131 nodes).
  - *Hard*: 6×6 grids with sparse checkpoints and complex labyrinthine walls (159–263 nodes).
- **Patches**:
  - *Easy*: 4×4 grids partitioned into 4 patches (all squares and wide rectangles).
  - *Medium*: 6×6 grids partitioned into 6–7 patches (combinations of squares, tall, and wide rectangles).
  - *Hard*: 6×6 grids with mixed shapes, freeform rectangular clues (`4F`), and partial clues (`?`, `T`) (up to 12 nodes).
- **Wend**:
  - *Easy*: 3×3 grid with 3-letter target words.
  - *Medium*: 4×3 grid with 3- and 4-letter target words.
  - *Hard*: 4×4 grid with 4- and 5-letter target words.
- **Crossclimb**:
  - *Easy*: 4 rungs with known `START` and `END` words.
  - *Medium*: 5 rungs with permuted intermediate words and known endpoints.
  - *Hard*: 6 rungs with missing `START` and `END` endpoints (`?`), requiring clue candidate filtering or dictionary neighborhood search.
- **Pinpoint**:
  - *Easy*: 4–5 specific clues matching a single category with >= 90% confidence.
  - *Medium*: 3 clues testing confidence margins against competing candidate hypotheses.
  - *Hard*: 2 clues or held-out out-of-lexicon categories testing conservative unknown-category detection.

### Aggregate Performance Table:

| Game | Tier | Puzzles | Solved/Valid | 1st Sol Time | Verif Nodes | Verif Time | Formal Problem Formulation |
|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---|
| **Queens** | Easy | 3 | 3/3 (100%) | 0.059 ms | 18.0 | 0.039 ms | $N$-Queens on Connected Color Regions with $L_\infty \ge 2$ Chebyshev Separation |
| **Queens** | Medium | 3 | 3/3 (100%) | 0.129 ms | 23.7 | 0.095 ms | $N$-Queens on Connected Color Regions with $L_\infty \ge 2$ Chebyshev Separation |
| **Queens** | Hard | 3 | 3/3 (100%) | 30.507 ms | 285.0 | 22.473 ms | $N$-Queens on Connected Color Regions with $L_\infty \ge 2$ Chebyshev Separation |
| **Mini Sudoku** | Easy | 3 | 3/3 (100%) | 0.233 ms | 1.0 | 0.233 ms | $6\times 6$ Latin Square with $2\times 3$ Box Constraints & Preserved Givens |
| **Mini Sudoku** | Medium | 3 | 3/3 (100%) | 0.139 ms | 1.0 | 0.139 ms | $6\times 6$ Latin Square with $2\times 3$ Box Constraints & Preserved Givens |
| **Mini Sudoku** | Hard | 3 | 3/3 (100%) | 0.264 ms | 2.0 | 0.264 ms | $6\times 6$ Latin Square with $2\times 3$ Box Constraints & Preserved Givens (Branches) |
| **Tango** | Easy | 3 | 3/3 (100%) | 0.129 ms | 1.0 | 0.129 ms | Binary Grid CSP ($N/2$ Parity, Trio Avoidance) with $=$ and $\times$ Edge Clues |
| **Tango** | Medium | 3 | 3/3 (100%) | 0.275 ms | 2.0 | 0.275 ms | Binary Grid CSP ($N/2$ Parity, Trio Avoidance) with $=$ and $\times$ Edge Clues |
| **Tango** | Hard | 3 | 3/3 (100%) | 0.102 ms | 17.0 | 0.102 ms | Binary Grid CSP ($N/2$ Parity, Trio Avoidance) with $=$ and $\times$ Edge Clues |
| **Zip** | Easy | 3 | 3/3 (100%) | 0.088 ms | 26.7 | 0.088 ms | Grid Hamiltonian Path with Wall Obstacles & Ascending Checkpoints |
| **Zip** | Medium | 3 | 3/3 (100%) | 0.100 ms | 81.0 | 0.100 ms | Grid Hamiltonian Path with Wall Obstacles & Ascending Checkpoints |
| **Zip** | Hard | 3 | 3/3 (100%) | 0.230 ms | 202.3 | 0.230 ms | Grid Hamiltonian Path with Wall Obstacles & Ascending Checkpoints |
| **Patches** | Easy | 3 | 3/3 (100%) | 0.220 ms | 5.0 | 0.220 ms | Exact Grid Cover by Disjoint Solid Rectangles Enclosing Clue Anchors |
| **Patches** | Medium | 3 | 3/3 (100%) | 0.254 ms | 4.3 | 0.254 ms | Exact Grid Cover by Disjoint Solid Rectangles Enclosing Clue Anchors |
| **Patches** | Hard | 3 | 3/3 (100%) | 0.154 ms | 7.7 | 0.154 ms | Exact Grid Cover by Disjoint Solid Rectangles Enclosing Clue Anchors |
| **Wend** | Easy | 3 | 3/3 (100%) | 0.053 ms | 4.3 | 0.053 ms | Exact Vertex-Disjoint Orthogonal Path Cover Matching Lexicon Words |
| **Wend** | Medium | 3 | 3/3 (100%) | 0.052 ms | 5.3 | 0.052 ms | Exact Vertex-Disjoint Orthogonal Path Cover Matching Lexicon Words |
| **Wend** | Hard | 3 | 3/3 (100%) | 0.069 ms | 5.3 | 0.069 ms | Exact Vertex-Disjoint Orthogonal Path Cover Matching Lexicon Words |
| **Crossclimb** | Easy | 3 | 3/3 (100%) | 0.076 ms | 2.0 | 0.076 ms | Simple / Hamiltonian Path in Word Hamming Graph $H(W, \Sigma)$ with Clues |
| **Crossclimb** | Medium | 3 | 3/3 (100%) | 0.042 ms | 2.0 | 0.042 ms | Simple / Hamiltonian Path in Word Hamming Graph $H(W, \Sigma)$ with Clues |
| **Crossclimb** | Hard | 3 | 3/3 (100%) | 0.045 ms | 5.7 | 0.045 ms | Simple / Hamiltonian Path in Word Hamming Graph $H(W, \Sigma)$ with Clues |
| **Pinpoint** | Easy | 3 | 3/3 (100%) | 0.146 ms | 1.0 | 0.146 ms | Offline Ranked Hypothesis Classification over Weighted Token Lexicon |
| **Pinpoint** | Medium | 3 | 3/3 (100%) | 0.352 ms | 1.0 | 0.352 ms | Offline Ranked Hypothesis Classification over Weighted Token Lexicon |
| **Pinpoint** | Hard | 3 | 3/3 (100%) | 0.074 ms | 1.0 | 0.074 ms | Offline Ranked Hypothesis Classification over Weighted Token Lexicon |

### Pinpoint corpus results (in-lexicon retrieval, not unseen-puzzle accuracy):

> The shipped clue sets were written to match (or deliberately not match) the shipped lexicon, so 100 % here shows the plumbing works, not that Pinpoint is solved.
1. **In-Lexicon Target Categories** (denominator 6 clue sets whose category is in the shipped lexicon):
   - **Top-1 Accuracy**: $6 / 6$ (**100.0%**)
   - **Top-3 Accuracy**: $6 / 6$ (**100.0%**)
   - **Mean Confidence Margin** ($\Delta$): **0.62**
2. **Out-of-Lexicon / Weak-Evidence Categories** (denominator 3 clue sets whose category is not in the lexicon):
   - **Unknown Detection Recall (Abstention)**: $3 / 3$ (**100.0%**)
   - **False Positive Rate**: $0 / 3$ (**0.0%**)
   - **Decision Rule**: Correctly abstains (returns `STATUS: UNKNOWN`) when confidence is below the $0.50$ threshold.

**Peak Memory**: 74.19 MB across all 72 test fixtures.

### Running the Corpus Benchmark:
```bash
./build_release/corpus-benchmark fixtures/corpus
```

---


---

## Status at a glance (read this first)

Measured on 795 real archived puzzles (not shipped; see [`AUDIT.md`](AUDIT.md) for method, tables and samples):

| Game | Genuinely solves fresh inputs? | Mode status |
|:--|:--|:--|
| Queens, Mini Sudoku, Tango, Zip, Patches | **Yes**: 100/100 valid and verified unique per game, independent Python checker, 0 timeouts | Complete for real-size boards. Large synthetic Queens boards can exhaust a budget. |
| Wend | **Only with a big enough dictionary you supply**: 76/95 valid with a 197k-word list; the other 19 failed only because an answer word was missing from it | Complete given the dictionary; the built-in 1,253-word list is for tests only. |
| Crossclimb | **Supplied-endpoint mode: yes if you type in the answer words** (83/100; 17 failed on a missing endpoint word). | **Incomplete:** hidden-endpoint mode (`START: ?`) is ambiguous on 99/100 without candidate lists; **clue-to-answer solving is not in this repository** (a separate macOS prototype ranks the right ladder first for 8 of 48 unseen puzzles). |
| Pinpoint | **No, not for unseen categories**: 0 of 46 on puzzles whose category is outside the lexicon | **Incomplete:** lexicon retrieval only; a separate macOS prototype generates "words that come before/after X" categories (8 of 46). |

## Hints and step-by-step explanations

Every game also has `hint`, `deduce` and `explain` commands, which show *why* each move is forced:

| Command | What it does |
|:--|:--|
| `hint` | One sound move from the board as it stands. |
| `deduce` | Sound moves only; it stops with a partial board when it cannot prove a move. |
| `explain` | The full trace: forced moves first, then labelled guesses with the same rules applied inside each branch, then a separate uniqueness check. |

Pinpoint's `explain` shows, after each clue reveal, the leading **hypotheses** and how each one connects to every clue. It then answers, asks for the next clue, or abstains. See [`ALGORITHMS.md` §11](ALGORITHMS.md#11-explainable-solving) for one worked example per game and measured before/after results.

```bash
./build_release/solvedin tango hint fixtures/tango_line_patterns.txt
./build_release/solvedin tango explain fixtures/corpus/tango/hard/tango_hard_1.txt --max-steps 0
./build_release/solvedin zip explain fixtures/corpus/zip/hard/zip_hard_1.txt --max-steps 0
```

**Shipped solvers vs. Crossclimb experiments.** Everything above ships in this repository.

- **Crossclimb, shipped:** only supplied-word reconstruction, which orders answer words you already have.
- **Crossclimb, not shipped:** the clue-to-answer experiments (the v1, v2 and v2.1 ladder solvers and the v3 candidate scorer), their frozen holdouts and their results. They depend on macOS dictionary and embedding data that cannot be redistributed, and they solve only a few percent of unseen puzzles. `AUDIT.md` §8–§9 reports them as research results.

## What each solver can do with the shipped inputs

"Shipped inputs" = `fixtures/*.txt` plus the 72 locally generated boards in `fixtures/corpus/`. All of them were written or generated for this project; none is a copy of a real LinkedIn puzzle.

| Game | Reads | Needs besides the board | Returns | Honest status |
|:--|:--|:--|:--|:--|
| Queens | colour-region grid | nothing | one solution, or uniqueness proof with `--check-unique` | Solves all shipped fixtures. Large generated boards can exhaust the budget (`INCONCLUSIVE`), and which seeds do depends on the platform. |
| Mini Sudoku | 6×6 givens | nothing | solution (+ uniqueness) | Solves all shipped fixtures. |
| Tango | givens + `=`/`x` edge clues | nothing | solution (+ uniqueness) | Solves all shipped fixtures. |
| Zip | grid, numbered checkpoints, walls | nothing | path (+ uniqueness) | Solves all shipped fixtures; harder boards can take many nodes. |
| Patches | clue cells (size/shape/`?`) | nothing | rectangle tiling (+ uniqueness) | Solves all shipped fixtures and all 100 archived real boards (cell-coverage pruning; the worst archived board went from 1.4 billion nodes to 300). Still exponential in the worst case. |
| Wend | letter grid + word lengths | **a dictionary** (`--dict`; tiny default for the fixtures) | word cover (+ uniqueness) | Correct only relative to the dictionary: a missing answer word means `NO_SOLUTION`, a large dictionary can add extra valid covers. |
| Crossclimb (**incomplete mode**: no clue solving) | the **answer words** of the rungs (unordered) and the top/bottom words | dictionary; optional endpoint candidates | ordered ladder, or `MULTIPLE_SOLUTIONS` | **Supplied-endpoint mode** (`START:`/`END:` given) orders the rungs. **Hidden-endpoint mode** (`?`) only works if you also supply `START_CANDIDATES`/`END_CANDIDATES`; otherwise a large dictionary makes it ambiguous. It never reads clue text and cannot work out the answer words. |
| Pinpoint (**incomplete mode**: retrieval only) | up to five clue strings | **a category lexicon** (`--lexicon`; 10 built-in categories, `fixtures/pinpoint_lexicon.txt`) | ranked categories, or `UNKNOWN` | A keyword-overlap ranker: it can only name a category that is already in the lexicon and shares words with the clues. It is not a general category guesser. |

Example runs (all from the repository root after building):

```bash
./build_release/solvedin crossclimb solve fixtures/crossclimb_valid_unique.txt --stats        # supplied endpoints
./build_release/solvedin crossclimb solve fixtures/crossclimb_missing_endpoints.txt --stats   # hidden endpoints + candidate lists
./build_release/solvedin pinpoint guess fixtures/pinpoint_clues_drives.txt                    # in lexicon -> confident
./build_release/solvedin pinpoint guess fixtures/pinpoint_clues_weak.txt                      # not in lexicon -> UNKNOWN
```

## Portability notes

- Built with CMake and tested (Release and ASan/UBSan, 17/17) on macOS (Apple clang, libc++) and Linux aarch64 (g++ 15.2, libstdc++); see `LIMITATIONS.md` for exactly what was run.
- Generators are seeded, but `std::shuffle` / `std::uniform_int_distribution` are implementation-defined, so a seed gives *different boards* on different standard libraries. The test suite therefore avoids asserting properties of seeded boards.
- In a Release build `assert` is disabled by `-DNDEBUG`; `CMakeLists.txt` re-enables it for the test executables so the tests really check things.

## Algorithmic Architecture Treatise

For state representations, constraint propagation mechanics, complexity analyses and step-by-step solver traces for each of the eight games, see [`ALGORITHMS.md`](ALGORITHMS.md). `python3 scripts/trace_algorithms.py check` (set `PUZZLE_CLI=<path>` if the binary is not in `build_release/`) re-runs the Python mirror of each search against the C++ CLI and requires identical node counts on 20 fixtures.

## Limitations

See [`LIMITATIONS.md`](LIMITATIONS.md).

## Licence

MIT, see [`LICENSE`](LICENSE). "LinkedIn" and the game names are used only to describe which puzzles the rules model; this project is not affiliated with or endorsed by LinkedIn.
