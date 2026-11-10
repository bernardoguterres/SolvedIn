#!/usr/bin/env python3
"""
Tiny independent Python brute-force oracle for small LinkedIn Queens boards.
Compares C++ solver variants (baseline, mrv, bitset) against pure-Python ground truth
on a fixed-seed corpus covering:
- No solution boards
- Unique solution boards
- Multiple solution boards
"""

import sys
import subprocess
import tempfile
import os

def solve_queens_oracle(n, regions):
    """
    Pure brute-force reference solver.
    Finds all valid queen placements directly from game rules.
    """
    solutions = []
    col_used = [False] * n
    reg_used = set()
    queens = [-1] * n

    def backtrack(r):
        if r == n:
            solutions.append(list(queens))
            return
        for c in range(n):
            if col_used[c]:
                continue
            if r > 0 and abs(queens[r - 1] - c) <= 1:
                continue
            reg = regions[r * n + c]
            if reg in reg_used:
                continue

            col_used[c] = True
            reg_used.add(reg)
            queens[r] = c

            backtrack(r + 1)

            col_used[c] = False
            reg_used.remove(reg)
            queens[r] = -1

    backtrack(0)
    return solutions


def validate_solution_independently(n, regions, queens):
    """
    Independent Python validator verifying all rules directly.
    """
    assert len(queens) == n, f"Expected {n} queens, got {len(queens)}"
    # 1. Bounds & Row uniqueness
    rows = [q[0] for q in queens]
    cols = [q[1] for q in queens]
    assert sorted(rows) == list(range(n)), f"Row conflict or out of bounds: {rows}"
    # 2. Column uniqueness
    assert len(set(cols)) == n, f"Column conflict: {cols}"
    # 3. Region uniqueness
    regs = [regions[r * n + c] for r, c in queens]
    assert len(set(regs)) == n, f"Region conflict: {regs}"
    # 4. Adjacency: no two queens touch even at a corner
    for i in range(n):
        for j in range(i + 1, n):
            dr = abs(queens[i][0] - queens[j][0])
            dc = abs(queens[i][1] - queens[j][1])
            assert not (dr <= 1 and dc <= 1), f"Queens at {queens[i]} and {queens[j]} touch!"
    return True


# Fixed-seed corpus of small boards
CORPUS = [
    # 1. No solution board (4x4)
    {
        "name": "corpus_4x4_no_solution",
        "n": 4,
        "grid": [
            [0, 3, 3, 3],
            [3, 1, 3, 3],
            [3, 3, 2, 3],
            [3, 3, 3, 3]
        ]
    },
    # 2. Unique solution board (5x5)
    {
        "name": "corpus_5x5_unique",
        "n": 5,
        "grid": [
            [0, 0, 1, 1, 1],
            [0, 2, 2, 2, 1],
            [3, 3, 2, 4, 1],
            [3, 3, 4, 4, 4],
            [3, 3, 3, 4, 4]
        ]
    },
    # 3. Multiple solutions board (4x4)
    {
        "name": "corpus_4x4_multiple",
        "n": 4,
        "grid": [
            [1, 1, 2, 2],
            [1, 1, 2, 2],
            [3, 3, 4, 4],
            [3, 3, 4, 4]
        ]
    },
    # 4. Multiple solutions board (5x5)
    {
        "name": "corpus_5x5_multiple",
        "n": 5,
        "grid": [
            [1, 1, 2, 2, 2],
            [1, 3, 3, 3, 2],
            [4, 4, 3, 5, 5],
            [4, 4, 5, 5, 5],
            [4, 4, 4, 5, 5]
        ]
    },
    # 5. Unique solution board (6x6 with long diagonal)
    {
        "name": "corpus_6x6_unique_long_diagonal",
        "n": 6,
        "grid": [
            [0, 1, 1, 0, 0, 1],
            [0, 1, 1, 1, 1, 0],
            [2, 2, 2, 1, 2, 2],
            [3, 3, 3, 3, 0, 3],
            [4, 4, 4, 0, 4, 0],
            [0, 5, 0, 0, 0, 0]
        ]
    }
]


def parse_cpp_queen_positions(output):
    """
    Parses Queen positions '(r, c)' from C++ stdout.
    """
    queens_list = []
    current_solution = []
    lines = output.splitlines()
    parsing_positions = False

    for line in lines:
        line = line.strip()
        if line.startswith("--- Solution") or line.startswith("Solution found:") or line.startswith("Unique solution found:"):
            if current_solution:
                queens_list.append(current_solution)
                current_solution = []
            parsing_positions = False
        elif line.startswith("Queen positions (row, col):"):
            parsing_positions = True
        elif parsing_positions and line.startswith("(") and line.endswith(")"):
            inner = line[1:-1]
            parts = [int(p.strip()) for p in inner.split(",")]
            current_solution.append((parts[0], parts[1]))
        elif parsing_positions and not line.startswith("("):
            parsing_positions = False

    if current_solution:
        queens_list.append(current_solution)

    return queens_list


def main():
    bin_path = sys.argv[1] if len(sys.argv) > 1 else "./build/solvedin"
    if not os.path.exists(bin_path):
        print(f"Error: Executable not found at {bin_path}")
        sys.exit(1)

    print(f"Running Python Brute-Force Oracle Tests against: {bin_path}")

    total_tests = 0
    variants = ["baseline", "mrv", "bitset"]

    for case in CORPUS:
        name = case["name"]
        n = case["n"]
        flat = [case["grid"][r][c] for r in range(n) for c in range(n)]

        # 1. Oracle ground truth
        oracle_sols = solve_queens_oracle(n, flat)
        num_oracle_sols = len(oracle_sols)

        expected_status = "NONE" if num_oracle_sols == 0 else ("UNIQUE" if num_oracle_sols == 1 else "MULTIPLE")
        print(f"\n[Case] {name} (N={n}) -> Ground Truth Solutions: {num_oracle_sols} ({expected_status})")

        # Write temporary board file
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as tf:
            tf.write(f"{n}\n")
            for row in case["grid"]:
                tf.write(" ".join(map(str, row)) + "\n")
            board_file = tf.name

        try:
            for variant in variants:
                # Test ordinary solve mode
                cmd_single = [bin_path, "queens", "solve", board_file, "--solver", variant]
                p_single = subprocess.run(cmd_single, capture_output=True, text=True)
                assert p_single.returncode == 0, f"Failed single solve for {variant}: {p_single.stderr}"

                if num_oracle_sols == 0:
                    assert "No solution exists." in p_single.stdout, f"{variant} failed no solution check"
                else:
                    assert "Solution found:" in p_single.stdout, f"{variant} should have found solution"
                    found_queens = parse_cpp_queen_positions(p_single.stdout)
                    assert len(found_queens) == 1, f"Expected 1 solution in single mode, got {len(found_queens)}"
                    validate_solution_independently(n, flat, found_queens[0])
                    # Verify returned cols matches one of oracle solutions
                    cols = [c for _, c in sorted(found_queens[0])]
                    assert cols in oracle_sols, f"Returned solution {cols} not in oracle solutions {oracle_sols}"

                # Test --check-unique mode
                cmd_unique = [bin_path, "queens", "solve", board_file, "--solver", variant, "--check-unique"]
                p_unique = subprocess.run(cmd_unique, capture_output=True, text=True)
                assert p_unique.returncode == 0, f"Failed unique check for {variant}: {p_unique.stderr}"

                if num_oracle_sols == 0:
                    assert "No solution exists." in p_unique.stdout, f"{variant} failed uniqueness check (expected no sol)"
                elif num_oracle_sols == 1:
                    assert "Unique solution found:" in p_unique.stdout, f"{variant} failed uniqueness check (expected unique)"
                    found_queens = parse_cpp_queen_positions(p_unique.stdout)
                    assert len(found_queens) == 1
                    validate_solution_independently(n, flat, found_queens[0])
                    cols = [c for _, c in sorted(found_queens[0])]
                    assert cols == oracle_sols[0]
                else:
                    assert "Multiple solutions found (at least 2):" in p_unique.stdout, f"{variant} failed uniqueness check (expected multiple)"
                    found_queens = parse_cpp_queen_positions(p_unique.stdout)
                    assert len(found_queens) == 2, f"Expected exactly 2 solutions stopped, got {len(found_queens)}"
                    for sol in found_queens:
                        validate_solution_independently(n, flat, sol)
                        cols = [c for _, c in sorted(sol)]
                        assert cols in oracle_sols

                print(f"  [OK] Variant '{variant}' perfectly matched Oracle")
                total_tests += 1

        finally:
            if os.path.exists(board_file):
                os.remove(board_file)

    print(f"\nAll {total_tests} Python Oracle comparison checks passed successfully!")


if __name__ == "__main__":
    main()
