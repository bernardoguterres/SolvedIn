# Algorithms: how each of the eight solvers works

> **Scope of this export.** Everything here is reproducible from the files in this repository (fixtures, CLI, `scripts/trace_algorithms.py`). A few paragraphs quote measurements taken earlier on real archived puzzles and on macOS-only prototypes that are *not* shipped; those are marked **(not reproducible from this repository)**. Executables are still named `solvedin`; see `LIMITATIONS.md`.

Audited against the C++ source on 2026-11-09. Read this file alone and you should be able to explain each solver without opening the code.

**How this document was checked.** Every worked trace below was produced by [`scripts/trace_algorithms.py`](scripts/trace_algorithms.py), a separate Python re-implementation of the C++ search order. `python3 scripts/trace_algorithms.py check` runs it against the real CLI on 20 fixtures and requires the same node count; all 20 matched on the audit run (Queens 13 and 10, Sudoku 4 and 17, Tango 1/4/7/42, Zip 28/11/185, Patches 5/11/11, Wend 4/7/5, Crossclimb 3/4/3). A mismatch means the C++ search order changed and the trace must be regenerated. Output you can reproduce is labelled with its command; anything I could only establish by reading code and not by running is marked *(code reading)*.

**Names checked against the source.**
- **AC-3 is not used anywhere** (`grep -ri "ac-3\|arc" src include` finds nothing). Earlier drafts of this file called Sudoku propagation "AC-3". It is plain *naked-single + hidden-single propagation to a fixed point*.
- **MRV** (fewest remaining options first) is used by Queens (`mrv` and `bitset`), Sudoku, Patches and Wend. **Tango does not use MRV**: it branches on the first undecided cell in row-major order. **Zip does not use MRV**: it uses a Warnsdorff-style ordering of next moves.
- **LCV** (least constraining value first) exists only in Queens `mrv` and `bitset`: candidates are sorted by how many currently-available cells they block.
- **Forward checking** is used in Queens (before descending) and, in a different form, in Zip (flood fill + degree test).

---

## Contents
1. [Shared conventions](#1-shared-conventions)
2. [Queens](#2-queens)
3. [Mini Sudoku](#3-mini-sudoku)
4. [Tango](#4-tango)
5. [Zip](#5-zip)
6. [Patches](#6-patches)
7. [Wend](#7-wend)
8. [Crossclimb](#8-crossclimb)
9. [Pinpoint](#9-pinpoint)
10. [Walkthrough table and reproduction commands](#10-walkthrough-table)
11. [Explainable solving: hint, deduce, explain](#11-explainable-solving)

---

## 1. Shared conventions

**Result status.** Every solver returns one of `UNIQUE_SOLUTION`, `MULTIPLE_SOLUTIONS`, `NO_SOLUTION`, `INCONCLUSIVE`. `NO_SOLUTION` means the search space was exhausted. `INCONCLUSIVE` means a budget stopped the search first; it is never reported as `NO_SOLUTION`.

**Finding one vs. checking uniqueness.** Without `--check-unique` the search stops at the first solution (reported `UNIQUE_SOLUTION` even though uniqueness was not examined). With `--check-unique` the solver sets `max_solutions = 2` and keeps searching until a second solution appears (`MULTIPLE_SOLUTIONS`) or the tree is exhausted (`UNIQUE_SOLUTION`). Wend and Crossclimb's `solve()` report by solution count only, so `UNIQUE_SOLUTION` there also means "exactly one found before the limit of 1 or 2".

**Budgets.** `--max-nodes N` stops when `nodes_visited >= N`; `--max-time-ms T` is checked every 256 nodes (Queens, Sudoku, Tango) or 1024 nodes (Zip, Patches, Wend, Crossclimb). A *node* is one call of the recursive search function; it is incremented on entry, so a run that finds its answer on the first call reports 1 node. Nodes from different games are not comparable.

**Independent checking.** Each game has a validator that re-checks a finished answer from the board rules alone, sharing no search state:

| Game | Validator | Called by the solver on each leaf? |
|:--|:--|:--|
| Sudoku, Tango, Zip, Patches | `validate_*_solution` | **Yes**: a leaf failing validation is discarded |
| Queens, Wend, Crossclimb | `validate_queens_solution`, `WendValidator`, `CrossclimbValidator` | **No**: used by the test suite and available to callers; the CLI does not call them (checked: no call in `main.cpp` or the solver `.cpp`). |

The validators live in the same code base, so this is independent *logic*, not an independent *program*. The only truly separate check is [`tests/oracle.py`](tests/oracle.py), a Python brute-force solver, used for small Queens boards. For Giant Queens I also wrote a separate Python validator and ran it on both 128×128 solutions (section 2.8).

---

## 2. Queens

Source: [`src/queens_solver.cpp`](src/queens_solver.cpp) (`solve_baseline`, `solve_mrv`, `solve_bitset`, `backtrack_bitset_impl`, `bitset_has_wipeout`), [`include/solvedin/bitset.hpp`](include/solvedin/bitset.hpp), validator [`src/queens_validator.cpp`](src/queens_validator.cpp).

### 2.1 Rules, input, output
Place exactly one queen in every row, every column and every colour region of an N×N board with N regions, with no two queens touching, diagonals included (Chebyshev distance ≥ 2).
- **Input:** `N`, then N rows of N region labels. Labels may be any N distinct integers (generated boards use 0..N-1, hand-written fixtures use 1..N); the solver maps them to indices 0..N-1 in order of first appearance.
- **Output:** queen coordinates, a status, node count, elapsed ms, budget status.

### 2.2 Variants
`--solver baseline` (default), `mrv`, `bitset`.
- `baseline`: row by row, checking each new queen against previous rows' columns and adjacent columns. No look-ahead.
- `mrv`: picks the most constrained row/column/region, with forward checking and LCV, on ordinary arrays.
- `bitset`: **same search order as `mrv`**, but all sets are multiword bitsets. Because the two visit identical node counts, any speed difference between them is cost per node, not a smarter search.

### 2.3 Search state (bitset variant)
- **Cell → bit.** `bit = r*N + c`. In the multiword bitset it lives in word `bit >> 6` at offset `bit & 63`. Example, 128×128 cell (100, 70): bit 12 870, word 201, offset 6; the whole board is 16 384 bits = 256 words = 2 KB per bitset.
- `row_mask[r]`, `col_mask[c]`, `reg_mask[k]`: bitsets of the N (or region-size) cells in each unit.
- `blocked_mask[cell]`: the row ∪ column ∪ region of `cell` ∪ its up-to-8 neighbours, i.e. every cell that becomes illegal once a queen sits on `cell`.
- `available_cells`: cells that can still hold a queen. Passed by value-copy down the recursion, so **undoing a placement means returning from the call**; only the `unassigned_*` sets and `current_queens` need explicit restore.
- `unassigned_rows/cols/regs`: which rows, columns, regions still lack a queen.
- Memory: `blocked_mask` is N² bitsets of N² bits. For N = 128 that is 16 384 × 2 KB = 32 MB. Measured peak RSS (`/usr/bin/time -l`): 71.3 MB (seed 42) and 37.6 MB (seed 1) for a 128×128 solve.

### 2.4 What happens, in order
1. Parse and build the masks above.
2. `backtrack_bitset_impl(available)`: count the node; stop if a budget is exhausted or enough solutions are held.
3. If N queens are placed, record the solution and return.
4. **Most constrained group.** For every unassigned row, column and region compute `popcount(available & unit_mask)`. If any count is **0** the branch is dead: return (this is the empty-domain rejection). Otherwise take the unit with the smallest count (first one wins ties; rows, then columns, then regions).
5. **Candidates.** `legal = available & unit_mask`. For each set bit (`for_each_set_bit`), tentatively remove its row/column/region from the unassigned sets and test `bitset_has_wipeout(available & ~blocked_mask[cell])`: does any *other* unassigned row/column/region now have no available cell? If so the candidate is rejected *before* any recursion.
6. **Order.** Survivors are sorted by ascending `popcount(available & blocked_mask[cell])` (how many live cells the queen would kill) — the LCV rule. For N > 32 an extra penalty term is added for neighbouring cells of other open regions, and for N ≥ 64 with every region ≥ N/2 cells, units are compared by ratio rather than count (`use_ratio`, `w_other` in `solve_bitset`). These are size-specific tunings.
7. For each candidate: push the queen, clear its three unassigned bits, recurse with `available & ~blocked_mask[cell]`, then **undo**: set the bits back, pop the queen.

```cpp
void backtrack_bitset_impl(ctx, available) {
  ctx.nodes_visited++;  if (budget hit || enough solutions) return;
  if (queens.size() == n) { save sorted solution; return; }
  (type, idx) = argmin over unassigned units of popcount(available & mask);
  if any such count == 0: return;                      // empty domain
  for cell in set_bits(available & unit_mask):
      if !bitset_has_wipeout(available & ~blocked[cell]) // with cell's units removed
          cands.push({cell, popcount(available & blocked[cell])});
  sort cands ascending by score;
  for cand in cands: place; recurse(available & ~blocked[cell]); undo;
}
```

### 2.5 Worked trace (verified): [`fixtures/valid_4x4_multiple.txt`](fixtures/valid_4x4_multiple.txt)
Board: four 2×2 quadrant regions 1 | 2 over 3 | 4. Command: `python3 scripts/trace_algorithms.py trace queens fixtures/valid_4x4_multiple.txt` (13 nodes, matches `queens solve … --solver bitset --check-unique --stats`).

```text
[node 1] MRV picks row 0 (4 legal cells); forward-check prunes none;
         LCV order (0,0):8, (0,3):8, (0,1):9, (0,2):9
  place queen (0,0)
  [node 2] MRV picks row 1 (2 legal cells); forward-check prunes (1,2); LCV order (1,3):5
    place queen (1,3)
    [node 3] MRV picks row 2 (1 legal cell); forward-check prunes (2,1); LCV order []
    undo queen (1,3)
  undo queen (0,0)
  place queen (0,3)  ... node 4-5: same shape, dead end ...  undo queen (0,3)
  place queen (0,1)
  [node 6] row 1 (1 legal) -> (1,3)  [node 7] row 2 -> (2,0)  [node 8] row 3 -> (3,2)
  [node 9] all 4 queens placed -> SOLUTION [(0,1),(1,3),(2,0),(3,2)]
  ... undo back to the root ...
  place queen (0,2) ... [node 13] SOLUTION [(0,2),(1,0),(2,3),(3,1)]
```
- **LCV scores:** at the root nothing is blocked; a queen on (0,0) blocks row 0 (4) + column 0 (4) + region 1 (4) + its neighbours, a union of **8** cells; (0,1) blocks **9** because its diagonal neighbour (1,2) lies outside its row/column/region. Lower score is tried first, so the order is (0,0), (0,3), (0,1), (0,2).
- **Rejecting an empty domain:** after (0,0), (1,3) the only cell left in row 2 is (2,1). Placing it would leave row 3 with no available cell, so forward checking rejects it; no candidate survives, node 3 returns, and the solver **undoes** (1,3) and then (0,0).
- **Forced move:** at node 6 row 1 has exactly one legal cell, so there is nothing to choose.
- **First vs. proof of uniqueness:** the first solution is found at node 9. The search only continued to node 13 because `--check-unique` asks for a second solution; finding it ends the run with `MULTIPLE_SOLUTIONS`. Without `--check-unique` the run stops at node 9.

(Both the empty-count check in step 4 and the forward check in step 5 reject empty domains; in this trace only the forward check fired, because forward checking catches the problem one placement earlier.)

### 2.6 Why the pruning is safe; checking; failure modes
- *Empty domain / wipeout:* every unassigned row, column and region needs a queen and queens only ever remove cells, so a unit with no available cell can never be satisfied. Rejecting a candidate whose placement empties another unit loses no solution. Both rules are necessary conditions only; they never claim a solution exists.
- *LCV ordering* changes only the order of siblings, never which siblings exist, so it cannot lose solutions — it only changes how soon the first one is found (and, with `--check-unique`, which two are found).
- *Checking:* `validate_queens_solution` (count, one per row/column/region, pairwise Chebyshev ≥ 2). Tests plus `tests/oracle.py` cross-check small boards. Not called inside the solver (section 1).
- *Ambiguity:* two solutions → `MULTIPLE_SOLUTIONS`. *Timeout:* `INCONCLUSIVE`. *Invalid input:* the parser rejects wrong token counts, non-integers and a region count ≠ N; it does not require labels to be 1..N, and I did not find a connectivity check in the parser *(code reading)*.

### 2.7 Time, memory, and what the benchmark measures
- Cost per node: O(N) popcounts over N²/64-word bitsets for unit selection, plus one wipeout test per candidate (3N more intersection tests). Mask construction is O(N²) bitsets × N²/64 words, i.e. O(N⁴/64) time and memory — the 32 MB above.
- Worst case is exponential. Solver tuning is size-specific, and some instances are hard: in the current `giant-queens` run, 32×32 seed 42 and 128×128 seed 7 exhaust the budget (200 000 nodes / 3 s) and are reported `INCONCLUSIVE`.
- Benchmark rows time `solve()` only (parsing and printing excluded) in a Release build and report nodes from the same definition for every variant.

### 2.8 Numbers reproduced on 2026-11-09 (Release build, macOS / Apple clang / libc++)

> **Seeds are not portable across standard libraries.** `queens generate --seed S` uses `std::shuffle` and `std::uniform_int_distribution`, whose output is implementation-defined, so the *same seed makes a different board* on Linux/libstdc++ than on macOS/libc++. The tables below are for the macOS boards. Checked on Linux (g++ 15.2, libstdc++, aarch64): 128×128 seed 42 **exhausts the 200 000-node budget in 2.1 s without a solution**, seed 1 solves in 129 nodes (42 ms), and 20×20 seed 777 solves with `mrv` and `bitset` in 21 nodes while `baseline` still hits the 200 000-node limit. Compare algorithms on the same generated file, not on the same seed across platforms.
128×128 boards from `queens generate --size 128 --seed S`, `--solver bitset --max-nodes 200000 --max-time-ms 3000`:

| Seed | First solution | Uniqueness search (`--check-unique`) | Independent validation |
|:--|:--|:--|:--|
| 42 | 737 nodes, 51.5 ms | 739 nodes, 50.6 ms → `MULTIPLE_SOLUTIONS` | 9/9 checks pass |
| 1 | 1161 nodes, 50.3 ms | 1163 nodes, 51.0 ms → `MULTIPLE_SOLUTIONS` | 9/9 checks pass |

The validator was a separate Python script (grid shape, N distinct labels, each region 4-connected, N queens, one per row/column/region, no touching pair, printed grid equals coordinate list). The seed-42 board is byte-identical to the archived `board_128x128.txt`. Uniqueness is **disproved** (a second placement exists), not proved.

20×20 seed 777 (`queens generate --size 20 --seed 777`), `--max-nodes 200000`:

| Variant | Outcome | Nodes | Time |
|:--|:--|:--|:--|
| `baseline` | **hit the 200 000-node limit, did not solve** | 200 000 | ≈7 ms |
| `mrv` | solved | 23 | median 0.060 ms (7 runs) |
| `bitset` | solved | 23 | median 0.048 ms (7 runs) |

A speed-up ratio is meaningful only between runs that both finish: `bitset` vs `mrv` ≈ 1.25×, identical node counts, so it is pure storage cost. No ratio against `baseline` exists. (Earlier versions of this file printed 23.9× / 88.3× / 69.9×; those divided a time-to-budget by a time-to-solution and are withdrawn.)

---

## 3. Mini Sudoku

Source: [`src/sudoku_solver.cpp`](src/sudoku_solver.cpp) (`propagate`, `backtrack_sudoku`), validator [`src/sudoku_validator.cpp`](src/sudoku_validator.cpp).

**Rules.** 6×6 grid, six boxes of 2 rows × 3 columns. Fill blanks with 1–6 so each row, column and box has each digit once; givens are fixed. **Input:** `6`, then six rows of six tokens (`.` or 1–6). **Output:** the grid(s), status, nodes.

**State.** `grid[6][6]` (0 = empty). `candidates[r][c]`: a byte whose bit *d* (1–6) says "digit d still possible here" (`0b01111110` initially; a given is just the single bit). `PEERS[r][c]`: the 14 other cells sharing its row, column or box.

**Key invariant.** For every cell, the candidate mask contains every digit that appears in *some* solution consistent with the assignments so far. Elimination removes only digits already placed in a peer, so no solution is ever removed.

**Order of events.**
1. Initialise masks; for each given remove its digit from its 14 peers (an emptied peer ⇒ contradictory givens ⇒ `NO_SOLUTION`, 0 nodes).
2. `propagate` loops until nothing changes: (a) *naked singles*: an empty cell with one candidate gets it and the digit is removed from its peers; (b) *hidden singles*: for each row, then column, then box, a digit with exactly one possible cell is placed there. A digit with zero possible cells, or any emptied cell, ⇒ contradiction.
3. `backtrack_sudoku` (one node per call): if full, record; else pick the empty cell with the fewest candidates (first in row-major order on ties — this is MRV) and try its digits ascending on a copy of the state, propagating after each.

```cpp
propagate(grid, cand):
  do { changed=false;
       for each empty cell: k=popcount; if k==0 fail; if k==1 { set; strip digit from peers; changed }
       for unit in rows, cols, boxes: for d in 1..6:
            places = cells in unit whose mask has d
            if none fail; if one and cell empty { set; strip from peers; changed }
  } while (changed)
backtrack(grid, cand): nodes++; if full {save; return}
  (r,c)=empty cell with min popcount; for d in cand[r][c]: copy state, set d, strip peers, if propagate ok: recurse
```

**Worked trace (verified)** [`fixtures/sudoku_branching.txt`](fixtures/sudoku_branching.txt), `python3 scripts/trace_algorithms.py trace sudoku fixtures/sudoku_branching.txt` (4 nodes = CLI):
- *Forced moves from the givens (no search):* hidden single in row 0 puts 6 at (0,0); row 1 → (1,4)=5; row 4 → (4,2)=1 and (4,4)=6; row 5 → (5,1)=5 and (5,2)=6; column 0 → (3,0)=1; column 5 → (2,5)=1; box 2 → (3,2)=4.
- **Node 1 stalls.** MRV picks (0,4) with candidates {2,3}.
- *Try 2:* propagation fills (3,4)=3, (3,5)=2, (1,2)=2, (2,1)=2, (2,2)=3, (5,3)=2 and stalls at **node 2**, picking (0,1) ∈ {3,4}. Both digits lead to a contradiction (a cell left with no candidate), so each is undone, and node 2 returns empty-handed → **backtrack** to node 1.
- *Try 3:* (3,4)=2, (3,5)=3, **node 3** picks (0,1) ∈ {2,4}. Digit 2 propagates all the way to a full grid (**node 4, solution**); digit 4 would contradict (shown when `--check-unique` asks for a second solution).
- Final: unique solution, 4 nodes, matches CLI.

**Why safe / checking / failures.** Propagation only deletes digits forbidden by an already-placed peer or forced by pigeonhole (a digit with one home), so it never discards a solution. MRV changes order only. Every leaf passes `validate_sudoku_solution` (all cells 1–6, givens preserved, each row/column/box mask = `0b01111110`). Two solutions ⇒ `MULTIPLE_SOLUTIONS` (e.g. `sudoku_valid_multiple.txt`, 17 nodes); budget ⇒ `INCONCLUSIVE`; contradictory givens ⇒ `NO_SOLUTION`; wrong size or tokens is rejected by the parser.

**Cost.** State is two 36-element arrays copied per branch (no heap allocation in propagation). Propagation is O(passes × 36×18×6). The benchmark corpus is tiny (1–4 nodes), so timings (≈0.01–0.3 ms) are dominated by process overhead rather than search.

---

## 4. Tango

Source: [`src/tango_solver.cpp`](src/tango_solver.cpp) (`propagate_tango`, `backtrack_tango`), validator [`src/tango_validator.cpp`](src/tango_validator.cpp).

**Rules.** 6×6 grid of Sun (`S`) / Moon (`M`): three of each per row and column; no three equal symbols in a row horizontally or vertically; `=` between two neighbours means same, `x` means different; givens fixed. **Input:** `6`; `GRID` (6 rows); `HORIZONTAL` (6 rows × 5 tokens, clue between (r,c) and (r,c+1)); `VERTICAL` (5 rows × 6 tokens, clue between (r,c) and (r+1,c)). **Output:** grid(s), status, nodes.

**State.** `domains[r][c]` ∈ {`S`=1, `M`=2, both=3}; an emptied domain is a contradiction.

**Key invariant.** Domains only shrink and always contain every symbol the cell takes in some valid completion. Each rule below is implied by the rules of the game.

**Propagation, repeated to a fixed point**, in this order: (1) edge clues (`=` intersects the two domains; `x` forces the opposite of a decided neighbour); (2) row and column counts (if a line already has 3 Suns, every undecided cell becomes Moon, and vice versa; more than 3 of either ⇒ contradiction); (3) no-three-in-a-row for rows then columns (two equal symbols at positions `i,i+1`, `i+1,i+2` or `i,i+2` force the remaining cell to the opposite symbol).

**Search.** `backtrack_tango` picks the **first undecided cell in row-major order** (not MRV), tries `S` then `M` on a copy, propagating after each; a fully decided grid is validated by `validate_tango_solution` and kept only if valid. The count rule only fires when a line *reaches* 3, so under-filled lines are not detected by propagation; the leaf validator catches them.

**Worked trace (verified)** [`fixtures/tango_edge_clue_dependent.txt`](fixtures/tango_edge_clue_dependent.txt), `trace tango …` (4 nodes = CLI):
- *Forced by the initial propagation:* the vertical `x` between (3,4) and (4,4) with the given S at (3,4) forces (4,4)=M; column 0 already holds three Suns, so (3,0) and (5,0) become M.
- **Node 1 stalls**; branch on (0,1). *Try S:* row 0 then has three Suns (S at 0, 1, 5), forcing (0,2),(0,4)→M → contradiction → undo (**backtrack**). *Try M:* three-in-a-row forces (0,2)=S and the row-count rule (0,4)=M.
- **Node 2** branches on (1,1): *S* leads on to **node 3** (branch on (2,1)): *S* is refuted after a chain of forced moves; *M* triggers a long chain (`=` and `x` edges, row/column counts) that fixes every cell → **node 4, solution**. (1,1)=M is a contradiction.
- The sibling fixture [`tango_edge_clue_ignored.txt`](fixtures/tango_edge_clue_ignored.txt) is the same board with an edge clue removed: 7 nodes and `MULTIPLE_SOLUTIONS`, showing the clue is what makes the answer unique.

**Safety / checking / failures.** Each rule is a direct consequence of the rules, so no solution is lost; the leaf validator re-checks counts, runs, edge clues and givens. Ambiguity ⇒ `MULTIPLE_SOLUTIONS`; budget ⇒ `INCONCLUSIVE`; contradiction ⇒ `NO_SOLUTION`; malformed tables are rejected by the parser (`tango_malformed.txt`).

**Cost.** Domains are a 6×6 byte grid copied per branch. Most corpus puzzles solve at the root (1 node); the hardest corpus file needs 42 nodes. Because branching is not MRV, harder boards may need far more nodes than a smarter chooser would — an honest limitation.

---

## 5. Zip

Source: [`src/zip_solver.cpp`](src/zip_solver.cpp) (`ZipSolver::solve`, lambdas `cuts_off_unvisited` and `dfs`), validator [`src/zip_validator.cpp`](src/zip_validator.cpp).

**Rules.** On an R×C grid with numbered checkpoints 1..K and walls, find one orthogonal path through **every cell exactly once**, starting at checkpoint 1, hitting checkpoints in order, ending at checkpoint K, never crossing a wall. **Input:** `R C`; `GRID` with `.` or checkpoint numbers; optional `WALLS_H` (R-1 rows × C tokens, `-` = wall between (r,c) and (r+1,c)) and `WALLS_V` (R rows × C-1 tokens, `|` = wall between (r,c) and (r,c+1)). **Output:** the path, a step-number grid, status, nodes.

**State.** `visited`, `path` (head = last element), scratch `bfs_visited` with a generation counter (so no clearing between BFS runs), `next_cp` (the checkpoint the path must reach next).

**Key invariant.** The unvisited cells plus the head must still admit a Hamiltonian path from the head to checkpoint K that visits remaining checkpoints in order. The pruning tests are necessary conditions for that.

**Order of events.** `dfs(next_cp = 2)`: count node; if the path covers all cells, accept only if the head is checkpoint K and `next_cp == K+1`. Otherwise run `cuts_off_unvisited`:
1. BFS from the head's first free neighbour through unvisited cells; if it reaches fewer cells than remain, the free region is split ⇒ prune.
2. If checkpoint `next_cp` was not reached ⇒ prune.
3. **Degree test:** every unvisited cell needs ≥ 2 free neighbours (counting the head as one), except the end checkpoint, which needs ≥ 1; else it cannot be entered *and* left ⇒ prune.

Then generate moves: unvisited, wall-free neighbours; a numbered cell is allowed only if it is exactly `next_cp`, and K only when it is the last cell. Order: checkpoint cells first, then ascending count of onward free neighbours (Warnsdorff's rule). Recurse; undo on return.

**Worked trace (verified)** [`fixtures/corpus/zip/easy/zip_easy_1.txt`](fixtures/corpus/zip/easy/zip_easy_1.txt) (4×4; 1 at (3,0), 2 at (0,2), 3 at (3,2), 4 at (2,2); a horizontal wall under (0,1) and a vertical wall right of (1,2)). `trace zip …` = 28 nodes = CLI.
- Nodes 1–16 follow the spiral (3,0)→(2,0)→(1,0)→(0,0)→(0,1)→(0,2) [checkpoint 2]→(0,3)→(1,3)→(2,3)→(3,3)→(3,2) [checkpoint 3]→(3,1)→(2,1)→(1,1)→(1,2)→(2,2) [checkpoint 4, last cell] → **solution at node 16**. At each step Warnsdorff picks the move with fewest onward options, so most nodes have one move.
- Pruning that fired *after* the solution was found (the search continues because `--check-unique` needs to rule out a second path): node 17 (head (1,2)): cell (0,3) would have only 1 free neighbour ⇒ prune; node 18 (head (1,1)): (0,0) has 1 ⇒ prune; node 20: a dead end with no moves; node 21: (3,1) has 1; nodes 26–28: (1,1), (0,0) or (2,0) would be stranded. Nothing else completes ⇒ `UNIQUE_SOLUTION`.
- Without `--check-unique` the same board reports 16 nodes.

**Safety / checking / failures.** (1) A Hamiltonian completion must visit all unvisited cells, so they must all be connected to the head through unvisited cells. (2) An interior path cell has exactly one predecessor and one successor, so it needs two usable neighbours; the end needs one. Both are necessary, so they discard only dead branches. Leaves pass `validate_zip_solution` (length R·C, all distinct, adjacent steps without walls, checkpoints ascending). `zip_walls_blocked.txt` ⇒ `NO_SOLUTION` after 1 node; budget ⇒ `INCONCLUSIVE`; `zip_malformed.txt` rejected.

**Cost.** Each node does a BFS and a full-grid degree scan: O(R·C). Worst case is exponential; the corpus "hard" files take 159–263 nodes. Memory is O(R·C).

---

## 6. Patches

Source: [`src/patches_solver.cpp`](src/patches_solver.cpp) (`PatchesSolver::solve`), validator [`src/patches_validator.cpp`](src/patches_validator.cpp).

**Rules.** Split the R×C board into **solid rectangles** (squares included), each containing exactly one clue cell, covering every cell exactly once. A clue token can give an area (`4`), a shape (`S` square, `T` tall = h>w, `W` wide = w>h, `F` any rectangle), both (`6W`), or nothing (`?`). Non-rectangular regions are invalid. **Input:** `R C`, `GRID` of tokens or `.`. **Output:** patch tiling, status, nodes.

**State.** `candidates_per_clue[k]`: every legal rectangle for clue *k*, each with its cell list and a **128-bit mask** (`mask_lo` bits 0–63, `mask_hi` 64–127, bit = `r*C+c`). `covered_lo/hi`, `covered_count`, `clue_used[]`, `current_patches`.

**Key invariant.** At every node, the placed rectangles are disjoint, each holds exactly its own clue, every unused clue still has at least one candidate disjoint from `covered`, **and every uncovered cell lies inside at least one such candidate** (the cell-coverage prune, added during the SolvedIn audit; see 6.8).

**Order of events.**
1. Quick rejects: all sizes given and sum ≠ R·C; or minimum sizes (given area, 2 for `T`/`W`, else 1) sum to more than R·C ⇒ `NO_SOLUTION`.
2. **Enumerate candidates** per clue: all (h, w) whose area/shape fit (a clue with no area gets a size range leaving room for the others), every placement containing the clue cell, discarding rectangles that contain another clue. An empty list ⇒ `NO_SOLUTION` (0 nodes). Enumeration is budgeted by step count.
3. **Exact-cover search.** Each node: for each unused clue, filter candidates by `(mask & covered) == 0`; if any clue has none ⇒ prune; choose the clue with the fewest compatible candidates (MRV; stop early at 1); try each, update the masks, recurse, undo. When all clues are placed and `covered_count == R·C`, validate and record.

**Worked trace (verified)** [`fixtures/patches_valid_unique.txt`](fixtures/patches_valid_unique.txt) (6×6, nine clues). `trace patches …` = 11 nodes = CLI.
- Candidate counts after enumeration: `4S`(0,0): 1; `6W`(0,2): 2; `4T`(0,5): 1; `4F`(2,0): 2; `4`(2,3): 6; `?`(3,2): 4; `4S`(4,0): 2; `T`(4,2): 1; `6W`(4,3): 2.
- Nodes 1–8 are all **forced** (MRV sees a clue with exactly one compatible rectangle each time): 2×2 at (0,0); 2×3 at (0,2); 4×1 at (0,5); 2×2 at (2,0); 2×2 at (2,3); 2×2 at (4,0); 2×1 at (4,2); 2×3 at (4,3). Each placement shrinks the others' compatible lists.
- Node 9 handles the `?` clue with 2 left: 1×1 at (3,2) leaves cell (2,2) uncovered with no clue to take it, so it fails; 2×1 at (2,2) covers it → **solution at node 11** (`UNIQUE_SOLUTION`).
- Regression [`patches_regression_2x2.txt`](fixtures/patches_regression_2x2.txt): `3F` on a 2×2 board has no rectangle of area 3 that fits ⇒ `NO_SOLUTION` with 0 nodes.

**Safety / checking / failures.** Enumeration is complete for solid rectangles containing exactly one clue; the bitmask test is exact overlap; MRV and "a clue with no candidate" pruning are standard exact-cover reasoning. Leaves are checked by `validate_patches_solution` (solid bounding box, one clue per patch, shape/size match, exact partition). Budget ⇒ `INCONCLUSIVE` (also if enumeration runs out of budget).
**Limit *(code reading, not tested)*:** the masks hold only 128 bits and `make_mask` silently drops cells at index ≥ 128, and the parser has no size check. On boards over 128 cells overlaps involving those cells go undetected in search; the leaf validator would still reject a bad cover, so answers stay correct but search can waste effort or miss the valid cover's pruning. LinkedIn boards are small.

### 6.8 Profile and optimizations (measured during development and the SolvedIn audit)

**Board.** An 8×8 Patches board with 16 clues, 10 of them `?` or a bare shape (each has many candidate rectangles). It is a real archived puzzle ("#151") and is **not shipped** **(not reproducible from this repository)**.

**Step 1 – allocation (first profile, macOS `sample`, Release).** ~63 % of time in `malloc`/`free`, ~11 % in `memmove`, ~22 % in the search function: the MRV scan built a `std::vector` of compatible candidates for every unused clue at every node, and every placement copied a cell list. The scan now only counts, and `current_patches` is a stack of pointers. Same nodes and answers, 22 M → 125 M nodes/s; #151 went from 62.8 s to 11.1 s (still over a 10 s budget).

**Step 2 – node count (second profile).** After step 1, 99.7 % of samples were inside the search scan itself and the search still visited 1 388 596 707 nodes: the bottleneck was *how many nodes*, not the cost of each. The only pruning was "an unused clue has no compatible candidate". It did not notice an uncovered **cell** that no remaining candidate can cover. The scan now ORs together the masks of every compatible candidate of every unused clue and backtracks when some uncovered cell is outside that union (`cell_prune`, only enabled when the board has ≤ 128 cells, because cells past bit 127 have no mask bit).

*Why it is safe.* A complete cover puts every cell in some chosen rectangle, and every chosen rectangle is a compatible candidate of an unused clue at this node. A cell outside the union can therefore never be covered by any completion, so the branch contains no solution. The MRV rule (first clue with the strictly smallest compatible count), the candidate order and the leaf validator are unchanged, so the first solution found is the same; only dead branches are cut sooner. The scan no longer stops early at count 1, because the union needs every clue's candidates.

*Evidence.* On 114 boards (100 archived + 9 corpus + 5 fixtures), each run with and without `--check-unique` (228 runs), the pre-change and post-change binaries print identical statuses and identical tilings; the independent Python rule checker accepts all 100 archived answers. Budget behaviour: #151 with `--check-unique` went from 1 878 097 770 nodes (14.5 s) to 303 nodes (0.13 ms), same verdict (unique).

| Archived Patches boards (100) | Before step 2 | After step 2 |
|:--|:-:|:-:|
| Valid, verified unique | 99 | **100** |
| Timeouts at 10 s | 1 (#151) | **0** |
| Nodes median / p95 / max | 149 / 803 574 / 1 273 260 032 (budget hit) | 25 / 162 / 1 365 |
| Search ms median / p95 / max | 0.020 / 6.7 / 10 000 | 0.019 / 0.074 / 0.109 |

The worst case is unchanged and still exponential; this prune removed the dead-cell branches that dominated this family of boards. Reproducible here: the shipped Patches fixtures and `ctest`. The Python mirror in `scripts/trace_algorithms.py` includes the same prune; it matches the C++ node counts on the three shipped Patches fixtures and, outside this repository, on five archived boards (177, 859, 117, 1 365 and 303 nodes).

---

## 7. Wend

Source: [`src/wend_solver.cpp`](src/wend_solver.cpp) (`enumerate_candidate_paths`, `solve`), [`src/trie.cpp`](src/trie.cpp), validator [`src/wend_validator.cpp`](src/wend_validator.cpp).

**Rules.** Cover a letter grid (cells marked `.` are voids) with non-overlapping orthogonal paths; each path spells a dictionary word; the multiset of path lengths equals `LENGTHS`. **Input:** `R C`, `GRID`, `LENGTHS: …`, optional `--dict file` (default: the embedded lexicon; the trace script reads `fixtures/default_lexicon.txt` and its node counts match the CLI on the three Wend fixtures traced, which indicates the two agree for them — I did not diff the word lists). **Output:** words with cell paths, cover map, status, nodes.

**State.** A 26-way `Trie` of the dictionary. *Candidates*: every (word, cell path) found. `cell_to_candidates[cell]`. `cell_covered[]` (voids start covered). `remaining_lengths`: how many words of each length are still needed.

**Key invariant.** Placed paths are disjoint; each placed word has a length whose remaining count is positive; every uncovered cell has at least one usable candidate.

**Order of events.**
1. **Stage 1 (no budget):** from every non-void cell walk orthogonally without revisiting a cell, following only trie children; record a candidate whenever the prefix is a word whose length is among `LENGTHS`; stop at the longest requested length. **Different paths spelling the same word are separate candidates.**
2. Build `cell_to_candidates` and the length counter.
3. **Stage 2:** at each node pick the uncovered cell with the fewest usable candidates (not overlapping covered cells, length still needed); 0 ⇒ dead end; place each candidate in turn, undo after. Success when all cells are covered and all length counts are zero.

**Worked trace (verified)** [`fixtures/wend_repeated_letters.txt`](fixtures/wend_repeated_letters.txt), 4×4, `LENGTHS: 4 4 4 4`. `trace wend …` = 7 nodes = CLI.
```text
FEED / DEEP / BOOK / WALL rows  ->  stage 1 finds 6 paths: FEED, FEED, DEEP, DEEP, BOOK, WALL
[node 1] MRV: cell (2,0) has 1 usable path [BOOK]   place BOOK      (forced)
[node 2] MRV: cell (3,0) has 1 usable path [WALL]   place WALL      (forced)
[node 3] MRV: cell (0,0) has 2 usable paths [FEED, FEED]
   place first FEED -> [node 4] cell (0,2): DEEP forced -> [node 5] SOLUTION
   place second FEED -> [node 6] cell (1,0): DEEP forced -> [node 7] SOLUTION
```
The two FEED (and two DEEP) candidates trace different cells because E and E are repeated letters; that is why they are kept separately, and why the puzzle is genuinely ambiguous (`MULTIPLE_SOLUTIONS`). The easy fixture `wend_easy_1.txt` (FOR/VAN/PIG rows) is a pure forced chain in 4 nodes.

**Safety / checking / failures.** Trie pruning is safe because a prefix that is not in the dictionary cannot become a word; MRV and the 0-candidate rule are exact-cover reasoning; requiring a positive `remaining_lengths` is the length rule itself. `WendValidator` checks continuity, dictionary membership, length multiset and exact partition, but is not called by the solver. Budget ⇒ `INCONCLUSIVE` — **but Stage 1 has no budget**, so a very large grid or lexicon can run long before search starts. `wend_no_solution.txt` ⇒ `NO_SOLUTION`.

**Dictionary assumptions.** A Wend board is solvable only if *every* word of the intended answer is in the dictionary you pass (`--dict`). The embedded default lexicon (~1 250 words) is small and exists for the fixtures and tests; real boards need a much larger list that you supply. Two failure modes follow, and both were seen when this solver was tried on real boards with a 197 k-word list **(not reproducible from this repository)**: (1) an answer word missing from the list (inflections such as BARGAINED, PANELS; closed compounds) gives `NO_SOLUTION` even though the board is fine, which was 19 of 95 boards; (2) a large list admits rare words and can create extra valid covers (7 of 76 solved boards were not unique). "No solution" therefore always means "no cover *using this dictionary*", and "unique" means unique for that dictionary.

**Cost.** Stage 1 is exponential in word length but cut sharply by the trie; Stage 2 rescans all uncovered cells per node. On real boards with a 197 k-word list the CLI took roughly 90–120 ms per call, mostly loading the trie **(not reproducible from this repository)**; the search itself was ~0.1 ms.

---

## 8. Crossclimb

Source: [`src/crossclimb_solver.cpp`](src/crossclimb_solver.cpp), validator [`src/crossclimb_validator.cpp`](src/crossclimb_validator.cpp).

**What this solver is — and is not.** In the real game each rung is given as a *clue* and the player must work out the answer word. **This solver never reads clue text.** It receives the answer words already (`RUNGS:`), unordered, and works out the order of the ladder and, if needed, the missing start/end words. Parts that need outside knowledge are exactly: (a) the rung words themselves, and (b) for an unknown endpoint, either `START_CANDIDATES`/`END_CANDIDATES` supplied by the user or the dictionary. Full clue-to-answer solving is not implemented.

**Input** (e.g. [`crossclimb_valid_unique.txt`](fixtures/crossclimb_valid_unique.txt)): `WORD_LENGTH`, `START:` word or `?`, optional `START_CANDIDATES:`, `RUNGS:` (one word per line), `END:` or `?`, optional `END_CANDIDATES:`. **Output:** the ladder, status, nodes.

**State.** `rung_adj`: graph on the rungs with an edge when Hamming distance = 1. `start_pool`, `end_pool`. A bitmask of used rungs (32-bit, so more than 32 rungs is unsupported *(code reading)*).

**Order of events.**
1. Build the rung graph.
2. Build `start_pool`: the given word (must be in the dictionary and not a rung); else the candidates passing the same test; else every dictionary word one letter from some rung and not a rung. Same for `end_pool`. An empty pool ⇒ `NO_SOLUTION`.
3. For each start word and each rung one letter away, DFS: extend through unused adjacent rungs. When all K rungs are used, every end-pool word one letter from the last rung (and different from the start) completes a ladder. Stop at 1 or 2 solutions.

**Worked traces (verified).**
- [`crossclimb_valid_unique.txt`](fixtures/crossclimb_valid_unique.txt): `START COLD`, rungs `CARD WARD CORD`, `END WARN`. Rung graph: CORD–CARD, CARD–WARD. Only CORD is one letter from COLD, so the DFS is forced: CORD → CARD → WARD, and WARD touches WARN. Ladder `COLD → CORD → CARD → WARD → WARN`, 3 nodes, unique.
- [`crossclimb_ambiguous.txt`](fixtures/crossclimb_ambiguous.txt): `COLD`, rungs `GOLD HOLD`, `BOLD`. GOLD and HOLD are adjacent, and both touch COLD and BOLD, so *both* orders work: `COLD→GOLD→HOLD→BOLD` (node 2) and `COLD→HOLD→GOLD→BOLD` (node 4). `trace crossclimb …` = 4 nodes = CLI; status `MULTIPLE_SOLUTIONS`.
- [`crossclimb_missing_endpoints.txt`](fixtures/crossclimb_missing_endpoints.txt): endpoints `?` with candidate lists `COLD BOLD` / `WARN WARM`; 3 nodes, also ambiguous.

**Safety / checking / failures.** Adjacent rungs must differ in exactly one letter and every rung is used once, so the DFS enumerates exactly the legal orderings; the dictionary check on endpoints is the only lexical filter (rungs themselves are not checked against it). `CrossclimbValidator` re-checks length, distance-1 steps, permutation and candidate membership (not called by the solver). Budget ⇒ `INCONCLUSIVE`; the dictionary-neighbour generation is not budgeted. Malformed lengths are rejected (`crossclimb_malformed.txt`).

**Endpoint and dictionary assumptions.**
- *Fixed endpoints* (`START:` and `END:` given, rungs supplied): this is the mode the shipped fixtures exercise (`crossclimb_valid_unique.txt`, `crossclimb_ambiguous.txt`). The solver needs both endpoints in the dictionary; if either is missing from the list, the answer is `NO_SOLUTION` even when the ladder is right (on 100 real puzzles with a 197 k-word list, 17 failed only for this reason **(not reproducible from this repository)**).
- *Hidden endpoints* (`START: ?`, `END: ?`): the solver offers every dictionary word one letter from some rung and not itself a rung, so with a large dictionary almost every puzzle is ambiguous (99 of 100 real puzzles returned `MULTIPLE_SOLUTIONS` **(not reproducible from this repository)**). Without `START_CANDIDATES`/`END_CANDIDATES` this mode cannot identify the endpoints; those are exactly the words the real game derives from a combined clue the solver does not read. With the tiny default lexicon it works on `crossclimb_missing_endpoints.txt` because the candidate lists are supplied.
- *Clues:* the real puzzles supply a clue for every rung plus a top/bottom clue. They are not parsed or used; the rung answers must be typed in. **Clue-to-answer solving is not implemented in this repository.**

**Cost.** Hamiltonian-path search over K rungs, O(K!) worst case, trivial for K ≈ 4–8. I did not profile dictionary loading versus search.

---

## 9. Pinpoint

Source: [`src/pinpoint.cpp`](src/pinpoint.cpp) (`term_matches_clue`, `PinpointGuesser::guess`), [`include/solvedin/pinpoint.hpp`](include/solvedin/pinpoint.hpp).

**What it is.** Not a search: a keyword-overlap ranker over a *lexicon you supply*. **Input:** a clue file (`CLUES:` then one clue per line; lines containing a comma are split into several clues) and a lexicon (`CATEGORY:` / `TERMS:` comma-separated / optional `DESCRIPTION:`). `--lexicon file` replaces the default; **if the file cannot be opened the built-in 10-category lexicon is silently used instead.** `--threshold` (default 0.50). **Output:** status, ranked categories with supported and unsupported clues.

**Scoring.** For each category and clue: normalise (upper-case, keep letters/digits/spaces); the clue *matches* a term if they are equal, or the clue equals one whole word of the term, or the term equals one whole word of the clue. `confidence = supported clues / total clues`. Categories with zero supported clues are dropped. Sort by confidence (difference > 1e-6), then supported count, then name.

**Decision rule (exactly).**
- No candidates, or top confidence **< threshold** ⇒ `UNKNOWN` (abstain). Equality is *not* abstention: exactly 0.50 is answered.
- Else if the top two have equal confidence and equal supported count ⇒ `AMBIGUOUS / TIED GUESS`.
- Else `CONFIDENT GUESS`. ("Confident" is only the status label; it can be as low as 50%.)

**Worked examples (all run with `solvedin pinpoint guess`, default lexicon):**
- [`pinpoint_clues_drives.txt`](fixtures/pinpoint_clues_drives.txt) — FLOPPY, HARD, COMPACT, FLASH, SOLID STATE: all five equal terms of *Computer Storage Drives* ⇒ 5/5 = 100% ⇒ `CONFIDENT GUESS`.
- Mixed clues FLOPPY, HARD, BANJO, PLUTO: *Computer Storage Drives* 2/4 = 50% (FLOPPY, HARD), *Planetary Bodies* 1/4, *String Instruments* 1/4. Top 0.50 ≥ 0.50 ⇒ `CONFIDENT GUESS` at 50% — the borderline case.
- [`pinpoint_clues_weak.txt`](fixtures/pinpoint_clues_weak.txt) — SPACESHIP, QUANTUM, SUBMARINE, TELEPORTATION match no term ⇒ no candidates ⇒ `UNKNOWN`.
- Single clue DIAMOND: *Card Suits* and *Precious Gemstones* both 1/1 ⇒ `AMBIGUOUS / TIED GUESS`.

**Safety / checking / failures.** Abstention is a recall/precision trade-off, not a proof. There is no separate validator; correctness is measured against labelled data. An empty clue file or empty lexicon ⇒ `NO_SOLUTION`/unknown.

**What the benchmarks actually show.** The solver can only name categories that are in the lexicon, and the shipped lexicons are tiny and hand-written. Every Pinpoint number in this repository (corpus benchmark, tests) is *in-lexicon retrieval* on clue sets written to match those categories; it says nothing about unseen puzzles. When the answer is not in the lexicon, the intended behaviour is abstention (`UNKNOWN`), as in `pinpoint_clues_weak.txt`; a clue set that partly overlaps a wrong category can still produce a confident wrong answer (see the 50 % example above). In earlier development work on real archived puzzles, a lexicon built from earlier puzzles scored 0 of 25 on later ones because each category appeared only once **(not reproducible from this repository)**. Cost is a linear scan of the lexicon (microseconds).

---

## 10. Walkthrough table

All commands run from the repository root after `cmake -B build_release -S . -DCMAKE_BUILD_TYPE=Release && cmake --build build_release -j8`. Node counts are from the audit run; "mirror" is `python3 scripts/trace_algorithms.py trace <game> <fixture>`.

| # | Game | Fixture | Shows | Result | Nodes | Reproduce |
|:-:|:--|:--|:--|:--|:-:|:--|
| 1 | Queens | `fixtures/valid_4x4_multiple.txt` | MRV, LCV order, forward-check rejection, undo, first solution (node 9) vs second (node 13) | `MULTIPLE_SOLUTIONS` | 13 | `./build_release/solvedin queens solve fixtures/valid_4x4_multiple.txt --solver bitset --check-unique --stats` |
| 2 | Mini Sudoku | `fixtures/sudoku_branching.txt` | hidden singles, naked singles, MRV branch, contradiction, backtrack | `UNIQUE_SOLUTION` | 4 | `./build_release/solvedin sudoku solve fixtures/sudoku_branching.txt --check-unique --stats` |
| 3 | Tango | `fixtures/tango_edge_clue_dependent.txt` | edge clues, count rule, no-three, branch + contradiction | `UNIQUE_SOLUTION` | 4 | `./build_release/solvedin tango solve fixtures/tango_edge_clue_dependent.txt --check-unique --stats` |
| 4 | Zip | `fixtures/corpus/zip/easy/zip_easy_1.txt` | Warnsdorff order, degree prune, uniqueness proof | `UNIQUE_SOLUTION` | 28 (16 without `--check-unique`) | `./build_release/solvedin zip solve fixtures/corpus/zip/easy/zip_easy_1.txt --check-unique --stats` |
| 5 | Patches | `fixtures/patches_valid_unique.txt` | candidate counts, forced MRV placements, failed 1×1 | `UNIQUE_SOLUTION` | 11 | `./build_release/solvedin patches solve fixtures/patches_valid_unique.txt --check-unique --stats` |
| 6 | Wend | `fixtures/wend_repeated_letters.txt` | trie enumeration, duplicate-word paths kept, ambiguity | `MULTIPLE_SOLUTIONS` | 7 | `./build_release/solvedin wend solve fixtures/wend_repeated_letters.txt --check-unique --stats` |
| 7 | Crossclimb | `fixtures/crossclimb_ambiguous.txt` | rung graph, two valid orders | `MULTIPLE_SOLUTIONS` | 4 | `./build_release/solvedin crossclimb solve fixtures/crossclimb_ambiguous.txt --check-unique --stats` |
| 8 | Pinpoint | `fixtures/pinpoint_clues_drives.txt` and the 50% / tie / weak examples above | scoring and abstention | `CONFIDENT GUESS` (100%) | n/a | `./build_release/solvedin pinpoint guess fixtures/pinpoint_clues_drives.txt` |

Regenerate and check every trace: `python3 scripts/trace_algorithms.py check`.


---

## 11. Explainable solving

Every game now has three extra commands next to `solve`. They share one step format (`include/solvedin/reasoning.hpp`):

| Command | What it does | Stops with |
|:--|:--|:--|
| `hint <file>` | The first sound deduction from the board **as it stands** (edit the file to continue from a partial board) | one `FORCED` step, or `NO_HINT` when the next move would be a guess |
| `deduce <file>` | Sound deductions only, repeated | `SOLVED_BY_DEDUCTION`, or `STALLED` with the partial board |
| `explain <file>` | Deductions, then bounded guessing with the same deductions **inside every branch**, then a separate uniqueness search | `SOLVED_BY_DEDUCTION`, `SOLVED_WITH_FALLBACK`, `MULTIPLE_SOLUTIONS`, `NO_SOLUTION`, `INCONCLUSIVE` |

**Step kinds are kept apart.**
- `FORCED` is a deduction from a named rule.
- `GUESS` is a speculative branch.
- `DEAD END` is a branch that broke a rule.
- `SOLUTION` is a complete board accepted by the game's independent validator.
- `(verify-unique)` marks work done after the first solution, to prove there is no second one.
- `HYPOTHESIS` is a ranked suggestion in a semantic game.

**Metrics.** Each run reports the rules used and their counts, guesses and dead ends, and search nodes and elapsed time, with solution search and uniqueness verification counted separately. A board finished by forced moves alone is unique by construction (every move was forced), so it needs no verification nodes. When the solution sits on the last unexplored branch, the earlier dead ends already prove uniqueness.

**Options.**
- `--max-steps N` limits how many steps are printed. The default is 200; 0 prints all.
- `--no-unique` stops at the first solution.
- `--max-nodes` and `--max-time-ms` are budgets.
- `--basic-rules` (Sudoku) runs singles only, so you can measure what the stronger rules buy.

The original `solve` commands are unchanged: same statuses and node counts on all 373 benchmark boards, apart from four 10-second timeouts whose node count depends on machine load.

**Results.** On the LinkedIn archive (50 boards per grid game, inputs only; private, not in this repo) and the public corpus, with the same release build, machine and 10 s budget:

| Game | Solved by deduction alone | Needed fallback | Before (`solve --check-unique`) | Explain: guesses, time |
|:--|:-:|:-:|:--|:--|
| Tango | 50/50 archive, 7/9 corpus | 2 corpus | median 7, max 50 nodes | 0 guesses on the archive; max 4 guesses on the corpus (was 42 nodes); median 0.07 ms (was 0.03) |
| Queens | 50/50 archive | multi-solution corpus boards | median 514, max 11,587 nodes (baseline solver; bitset: 14.5 / 97) | 0 guesses on the archive; 8/9 corpus boards finish within 10 s. The 128×128 board does not finish, which bitset does in 50 ms: a regression on that stress board |
| Mini Sudoku | 50/50 archive | 1 corpus, 1 fixture | median 1 node | singles alone give the same outcome, guesses and nodes on all 62 boards; the extra rules cost about 8% time. On 399 sparse generated boards they finish 3 that singles stall on |
| Zip | 27/50 archive, 8/9 corpus | 23 archive, 1 corpus | median 324, max 42,121 nodes, worst 50.8 ms | max 48 guesses, worst 5.7 ms |
| Patches | 50/50 archive, 100/100 external set, all corpus | none | median 25, max 1,365 nodes (#151: 303 nodes) | 0 guesses everywhere; #151 in 17 forced steps, 0.6 ms |
| Wend | 43/50 archive (all 43 exact) | none | same statuses | 6 ambiguous because the dictionary admits other covers, 1 dictionary miss (BUZZWORD), 0 wrong, 0 search failures; 220/221 answer words are in the lexicon |

The two best demos of strategy:

```bash
./build_release/solvedin tango explain fixtures/corpus/tango/hard/tango_hard_1.txt --max-steps 0
./build_release/solvedin zip explain fixtures/corpus/zip/hard/zip_hard_1.txt --max-steps 0
```

The first shows every Tango rule, a stall, guesses, dead ends and the solution. The second solves a hard Zip entirely by corridors, checkpoint order and no-loop reasoning.

### 11.1 Tango

**Key move: line patterns.** `fixtures/tango_line_patterns.txt` has row 0 = `S ? ? ? ? S`.

```text
$ ./build_release/solvedin tango hint fixtures/tango_line_patterns.txt
[1] FORCED line-patterns: Row 0 (S . . . . S) has 2 legal arrangements given its symbols, signs, 3-and-3 count and the
    no-three rule: SMSMMS, SMMSMS. All of them agree on 2 open cells. A sun at (0, 1) would force three in a row.
    A sun at (0, 4) would force three in a row.
    => (0, 1) M, (0, 4) M
```

**Why it works.** A line is legal only if it has exactly 3 of each symbol, never three in a row, and respects the signs inside it. Enumerating the legal completions and placing only the values they all share is sound. Tango does **not** require rows to be distinct, so no rule uses that.

**Rules, cheapest first:**
- `equal-sign`, `opposite-sign`;
- `pair`, `gap` (no three in a row);
- `count`;
- `x-pair-count`: an open `x` pair supplies one of each symbol to its line;
- `equal-pair-count`: an open `=` pair would add two of the same symbol;
- `line-patterns`, only after the others stall.

**Data structures.** Each cell holds a 2-bit domain (S, M, both). Line patterns are enumerated depth-first with pruning, capped at 20,000 per line; a line over the cap is skipped and counted. A 16×16 board enumerates 2.66 M patterns in 0.22 s.

**Fallback.** Branch on the open cell of the line with the fewest legal arrangements, trying the value most arrangements use first.

**Before/after.** In the archive the old solver's first guess was always the top-left open cell. On #757 it guessed (0,0) = S with nothing propagated (the board was exactly its 6 givens and 9 signs) and used 50 nodes. The new solver fixes column 3 by line patterns and finishes with no guess.

```bash
./build_release/solvedin tango hint fixtures/tango_line_patterns.txt
./build_release/solvedin tango explain fixtures/corpus/tango/hard/tango_hard_3.txt --max-steps 0        # deduction only
./build_release/solvedin tango explain fixtures/corpus/tango/hard/tango_hard_1.txt --max-steps 0        # fallback used
```

### 11.2 Queens

**Key move: region-to-line lock** (`fixtures/valid_8x8_unique.txt`).

```text
[3] FORCED region-line-lock: Every remaining cell of region 4 ((4, 0), (4, 4), (4, 5)) lies in row 4, so region 4's
    queen will be in row 4: the other cells of row 4 are eliminated.
```

**Rules:**
- `queen-attacks`: a placed queen rules out its row, column and region, and the 8 cells touching it, diagonals included;
- `lone-candidate`: a row, column or region with one cell left;
- `region-line-lock`, and the reverse `line-region-lock`;
- `would-empty-group`: a cell whose queen would attack every remaining cell of another group;
- `region-group-lock` and `line-group-lock`: k regions confined to k lines, or the reverse, for k = 2 (boards up to 32) and k = 3 (up to 16).

**Data structures.** One cell state per cell (open, queen, out), precomputed region cell lists, and 64-bit masks for the group locks.

**Fallback.** Branch on the group with the fewest candidates, least-constraining candidate first. This reuses the bitset solver's group choice and candidate ordering. `solve --solver bitset` remains the fast solver for large generated boards.

```bash
./build_release/solvedin queens hint fixtures/valid_8x8_unique.txt
./build_release/solvedin queens explain fixtures/valid_8x8_unique.txt --max-steps 0
./build_release/solvedin queens explain fixtures/corpus/queens/easy/queens_8x8_101.txt --max-steps 0   # fallback, two solutions
```

### 11.3 Mini Sudoku

**Key move: naked pair** (`fixtures/sudoku_naked_pair.txt`; singles alone stall on this board).

```text
[5] FORCED naked-pair: (2, 1) and (2, 5) in row 2 can only hold 3,5, so those two digits are used up there:
    eliminate them from the other cells of row 2.
```

**Rules:**
- `naked-single`, which names the digits seen in the row, the column and the 2×3 box;
- `hidden-single`;
- `locked-box-line`: every place for a digit in a box lies on one line, so the digit is eliminated from that line outside the box;
- `locked-line-box`, the reverse;
- `naked-pair`.

**Data structures.** Digits on the grid, plus a per-cell bitmask of eliminated digits. Candidates are recomputed from those, so a placement never needs its own elimination step.

**Fallback.** The cell with the fewest candidates.

**Measured.** On every LinkedIn board, singles suffice: the stronger rules add about 8% time and change nothing. They matter on sparse boards (3 of 399 generated).

```bash
./build_release/solvedin sudoku explain fixtures/sudoku_naked_pair.txt --max-steps 0
./build_release/solvedin sudoku deduce fixtures/sudoku_naked_pair.txt --basic-rules       # singles only: STALLED
./build_release/solvedin sudoku explain fixtures/sudoku_locked_candidates.txt --max-steps 0   # locked candidates, then fallback
```

### 11.4 Zip

**Key move: checkpoint order** (`fixtures/corpus/zip/hard/zip_hard_1.txt`).

```text
[17] FORCED checkpoint-order: Linking (3, 4)-(4, 4) would put checkpoints 4, 2 on one stretch of path, out of order,
     so the link is closed.
```

**Model.** The path is a set of links between neighbours without a wall, each on, off or undecided. The start and end need one link; every other cell needs two. A partial path is a set of fragments with two free ends each, so no rule depends on a single moving head.

**Rules:**
- `corridor`: a cell with exactly as many usable links as it needs;
- `full-cell`;
- `no-loop`;
- `checkpoint-order`: a stretch of path must carry consecutive checkpoints in order;
- `early-finish`: start and end joined too early;
- `only-link`: a bridge between two areas must be used;
- contradictions: `dead-end`, and `stranded-area` (the usable links split the board, or a cut cell would leave more areas than it has links).

**Data structures.** An edge list with per-cell incidence, union-find-style fragment walks, and Tarjan bridges and articulation points.

**Fallback.** Binary branching on a link of the cell with the fewest open choices.

```bash
./build_release/solvedin zip hint fixtures/corpus/zip/hard/zip_hard_1.txt
./build_release/solvedin zip explain fixtures/corpus/zip/hard/zip_hard_1.txt --max-steps 0
./build_release/solvedin zip explain fixtures/corpus/zip/medium/zip_medium_3.txt --max-steps 0     # fallback used
```

### 11.5 Patches

**Key move: a cell only one rectangle can cover** (`fixtures/corpus/patches/hard/patches_hard_1.txt`).

```text
[9] FORCED only-cover: (2, 2) can only be covered by one rectangle: clue F ('?' at (3, 2)) as rows 2-3, columns 2-2 (2x1).
```

**Candidates.** Exactly the exact-cover solver's: solid rectangles containing their clue and no other, filtered by area and shape.

**Rules:**
- `single-rectangle`: a clue with one rectangle left;
- `only-cover`: a cell only one rectangle can cover;
- `reserved-cell`: a cell every rectangle of one clue covers, which removes other clues' rectangles through it;
- `only-clue`: a cell only one clue can reach, which constrains that clue to cover it;
- contradictions: `no-rectangle` and `stranded-cell`.

**Fallback.** The most constrained clue or cell.

**Before/after.** Patches #151 took 1.39 billion nodes originally, and 303 nodes after the earlier cell-coverage prune. It now takes 17 forced steps and no guess.

```bash
./build_release/solvedin patches hint fixtures/corpus/patches/hard/patches_hard_1.txt
./build_release/solvedin patches explain fixtures/corpus/patches/hard/patches_hard_1.txt --max-steps 0
```

### 11.6 Wend

**Key move: scarce cell** (`fixtures/wend_valid_unique.txt`).

```text
Note: every deduction assumes the dictionary (embedded test lexicon) is complete for this board ...
[1] FORCED scarce-cell: (0, 0) 'F' has 2 open neighbours, and only one dictionary word path still covers it:
    FOR (0, 0)-(0, 1)-(0, 2).
```

**Candidates.** The solver's trie-pruned word paths: orthogonal steps, no reused cell, only the displayed lengths, and a separate candidate for each path of the same word.

**Rules:**
- `scarce-cell`;
- `forced-length`: exactly as many disjoint paths of a length are left as the board still needs;
- `would-strand`: a one-step look-ahead that removes a path if it would strand a cell, use up a length, or wall off an area no combination of the remaining lengths can fill.

**The dictionary assumption is printed first.** A word missing from the dictionary produces `NO_SOLUTION`, never a wrong placement. `research/scripts/wend_eval.py` reports dictionary misses separately from search failures, and the blind benchmark uses only a dictionary built without answers.

```bash
./build_release/solvedin wend explain fixtures/wend_valid_unique.txt --max-steps 0
./build_release/solvedin wend explain fixtures/corpus/wend/easy/wend_easy_2.txt --max-steps 0    # fallback: two covers
```

### 11.7 Crossclimb

**Supplied-word reconstruction** (`explain` on a board with RUNGS) orders words you already have. It is not clue solving.

```text
[2] FORCED corridor: CARD is one letter from exactly two words (WARD and CORD), so they are its neighbours on the ladder.
COLD
CORD   (letter 3: L -> R)
CARD   (letter 2: O -> A)
WARD   (letter 1: C -> W)
WARN   (letter 4: D -> N)
```

The same link rules as Zip apply on the one-letter graph: corridor, full word, no loop, no early top-to-bottom link. A hidden endpoint is a node linked to every rung with a candidate one letter away. Clue solving remains a separate macOS research prototype (not shipped; see `LIMITATIONS.md`).

```bash
./build_release/solvedin crossclimb explain fixtures/crossclimb_valid_unique.txt --max-steps 0
./build_release/solvedin crossclimb explain fixtures/crossclimb_missing_endpoints.txt --max-steps 0   # hidden endpoints: two ladders
```

### 11.8 Pinpoint

**Category inference, shown after each clue reveal.** Each step is a `HYPOTHESIS` from one of two generators, reported separately:
- **Lexicon:** the editable category lexicon.
- **Compound:** an open words-before/after inference from a plain word list. It finds an affix X such that X+clue or clue+X is a word for every revealed clue, shows the connection (`FIRE + FLY = FIREFLY`) and the affix's breadth. Phrase clues are marked not testable, and a plural clue may match its singular form.

The decision is one of: answer, ask for the next clue, or abstain after the last clue. Never answering on one clue is a policy choice.

```bash
./build_release/solvedin pinpoint explain fixtures/pinpoint_clues_drives.txt                                  # lexicon: answers after 2 clues
printf 'Light\nHouse\nFly\n' > /tmp/fire.txt
./build_release/solvedin pinpoint explain /tmp/fire.txt --dict /usr/share/dict/words                         # compound hypotheses
```

**Measured, with each run seeing only the first k clues.**
- **Development puzzles:** every answer the compound generator would have given on its own was wrong (26 of 26). LinkedIn mostly uses open compounds and phrases ("safety pin"), which a word list cannot see. So compound hypotheses are shown but never answered.
- **Frozen unseen set** (46 puzzles, scored by the frozen equivalence judge): the explainer abstains at every clue count (k = 1..5), with 0 wrong answers. The correct before/after hypothesis appears in its top 3 for 1 puzzle at each k.
- **For comparison:** archive retrieval is lookup, not inference, and stays a separate mode.
