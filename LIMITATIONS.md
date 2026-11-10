# Limitations (read this before relying on any result)

`SolvedIn` is an **unofficial hobby/research project**, not affiliated with or endorsed by LinkedIn. It contains no real LinkedIn puzzle data.

## What the shipped inputs show
- Every fixture in `fixtures/` was written or generated for this project (`fixtures/corpus/` holds 72 locally generated boards). They are small and easy for a modern machine and **do not predict performance on real LinkedIn puzzles**.
- The test suite (17 CTest targets) and the Python mirror (`scripts/trace_algorithms.py check`, 20 fixtures) show that the solvers are internally consistent, that independent validators accept their answers on these fixtures, and that the documented traces match the code. They do not show correctness on arbitrary boards.
- The full audit with per-game samples, results and caveats is in [`AUDIT.md`](AUDIT.md).
- Earlier development work compared the grid-game solvers with ~100 real archived puzzles per game from third-party sites. Those puzzles are deliberately **not** included, so those results cannot be reproduced from this repository.

## Per-game status
- **Queens, Mini Sudoku, Tango, Zip, Patches**: exact backtracking solvers. Any answer returned is checked against the rules by the validators in the tests; a search budget (`--max-nodes`, `--max-time-ms`) ends in `INCONCLUSIVE`, never a false "no solution". Worst case is exponential: large Zip and Queens boards (especially synthetic ones) can exhaust a budget, and Patches boards with many `?` clues were the slowest family before the cell-coverage pruning (one archived board needed 1.4 billion nodes; it now needs 300).
- **Wend**: correct only relative to the dictionary you pass. A missing answer word gives `NO_SOLUTION`; a big dictionary can give valid-but-different covers. The built-in word list (~1 250 words) exists only for the fixtures.
- **Crossclimb**: the solver **does not read clues**. You must type in the rung answer words. *Supplied-endpoint mode* (`START:`/`END:` given) orders the rungs and is the mode the fixtures exercise. *Hidden-endpoint mode* (`?`) needs `START_CANDIDATES`/`END_CANDIDATES`; with only a large dictionary it is almost always ambiguous. Clue-to-answer solving is not implemented.
- **Pinpoint**: a keyword-overlap ranker over a lexicon you supply. It can only name categories that are already in the lexicon and share words with the clues. The shipped lexicons are tiny and hand-written; 100 % results in `corpus-benchmark` are in-lexicon retrieval on clue sets written to match, not accuracy on unseen puzzles. On a category outside the lexicon the intended behaviour is `UNKNOWN`, but partial overlap with a wrong category can give a confident wrong answer.

## Portability
- **Seeded generators are not portable across standard libraries.** They use `std::shuffle` / `std::uniform_int_distribution`, which are implementation-defined, so the same seed produces a different board on macOS (libc++) and Linux (libstdc++). Example: `queens generate --size 128 --seed 42` is solved in 737 nodes on macOS but exhausts a 200 000-node budget on Linux. Compare solvers on the same board file, not the same seed.
- Patches search masks hold 128 bits; boards over 128 cells are unsupported (the leaf validator still rejects bad covers). Crossclimb uses a 32-bit rung mask (≤ 32 rungs).
- Validators are not run inside every solver: Sudoku/Tango/Zip/Patches validate each leaf; Queens, Wend and Crossclimb return unvalidated answers (validators exist and the tests call them).
- Real LinkedIn Queens boards are ≤ 9×9; the giant-board benchmarks are synthetic stress tests.

## Tested environments (see the release notes of this export for the exact commands)
- macOS (Apple silicon, Apple clang, CMake): Release and ASan/UBSan, 17/17 CTest targets.
- Linux aarch64 (Ubuntu 26.04.1, g++ 15.2, libstdc++, CMake 4.2.3 from the Ubuntu package in a throwaway container): CMake Release and ASan/UBSan builds, 17/17 CTest targets each; `scripts/trace_algorithms.py check` 20/20.
- **Not tested**: x86-64 Linux, Windows/MSVC, CI, fuzzing or any "frame-breaking" campaign.
- g++ 15.2 prints three `-Wmaybe-uninitialized` warnings for `src/pinpoint.cpp` (the lexicon parser's `std::optional<PinpointCategory>`) at -O1 and above, none at -O0, none with Apple clang. Investigated: every use of the optional is guarded by `has_value()`, the warnings point into libstdc++'s inlined `optional` destructor/move-assignment, and on Linux 3,000 random lexicon files under valgrind (-O3 build, `--track-origins`) and 20,000 under ASan/UBSan report no uninitialised read or other error. This matches a known class of GCC false positives for `std::optional` of non-trivial types. Not changed.

## Not done
MIT licensed (see `LICENSE`). No CI, no packaged release, no fuzzing, no public API stability promise.

## Explainable solvers (`hint` / `deduce` / `explain`)

- **They are slower than `solve`.** They recompute candidates from scratch and write an explanation for every step. Medians on the LinkedIn archive:
  - Tango: 0.07 ms against 0.03 ms;
  - Mini Sudoku: 0.75 ms against 0.003 ms;
  - Queens: 0.1 ms against 0.02 ms.

  `solve` is unchanged and remains the fast path.
- **Queens stress boards.** The deduction-first fallback reuses the bitset solver's group choice and candidate order, but on the generated 128×128 corpus board it does not finish within 10 s, while `solve --solver bitset` takes 50 ms. Each node rescans the whole board, and the extra eliminations change the search order. Use `solve --solver bitset` for large generated boards.
- **Mini Sudoku.** Locked candidates and naked pairs never change the outcome on LinkedIn-style boards, which singles always finish. They help on sparse boards: 3 of 399 generated.
- **Rules are line-local or group-local.** Some generated Tango boards (2 of 9 in the corpus) and 23 of 50 archived Zip boards still need guesses after every rule has run.
- **Wend deductions assume the dictionary.** A word missing from it makes the board `NO_SOLUTION`; extra words can make it `MULTIPLE_SOLUTIONS`.
- **Pinpoint `explain` produces hypotheses, not proofs.**
  - The compound generator only sees closed compounds in the word list you pass (`FIRELIGHT`, not "fire drill"), so it is shown but never answered.
  - On a frozen unseen set it abstains on all 46 puzzles at every clue count. The right before/after hypothesis appears in its top 3 for 1 of 46.
