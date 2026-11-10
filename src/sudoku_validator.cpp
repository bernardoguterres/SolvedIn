#include "solvedin/sudoku_validator.hpp"

#include <string>

namespace solvedin {

ValidationResult validate_sudoku_solution(
    const SudokuBoard& initial,
    const std::array<std::array<int, SudokuBoard::SIZE>, SudokuBoard::SIZE>& solution) {

    constexpr int N = SudokuBoard::SIZE;

    // 1. Check all cells in range [1..6] and givens preserved
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int d = solution[r][c];
            if (d < 1 || d > N) {
                return {false, "Cell (" + std::to_string(r) + ", " + std::to_string(c) +
                               ") has invalid digit " + std::to_string(d) + " (must be 1-6)"};
            }
            if (initial.is_given(r, c) && initial.get(r, c) != d) {
                return {false, "Given digit " + std::to_string(initial.get(r, c)) +
                               " at (" + std::to_string(r) + ", " + std::to_string(c) +
                               ") was altered to " + std::to_string(d)};
            }
        }
    }

    // 2. Validate rows contain 1..6 exactly once
    for (int r = 0; r < N; ++r) {
        unsigned int mask = 0;
        for (int c = 0; c < N; ++c) {
            int d = solution[r][c];
            if (mask & (1u << d)) {
                return {false, "Row " + std::to_string(r + 1) + " contains duplicate digit " + std::to_string(d)};
            }
            mask |= (1u << d);
        }
        if (mask != 0b1111110u) {
            return {false, "Row " + std::to_string(r + 1) + " does not contain all digits 1-6"};
        }
    }

    // 3. Validate columns contain 1..6 exactly once
    for (int c = 0; c < N; ++c) {
        unsigned int mask = 0;
        for (int r = 0; r < N; ++r) {
            int d = solution[r][c];
            if (mask & (1u << d)) {
                return {false, "Column " + std::to_string(c + 1) + " contains duplicate digit " + std::to_string(d)};
            }
            mask |= (1u << d);
        }
        if (mask != 0b1111110u) {
            return {false, "Column " + std::to_string(c + 1) + " does not contain all digits 1-6"};
        }
    }

    // 4. Validate 2x3 boxes contain 1..6 exactly once
    for (int b = 0; b < SudokuBoard::NUM_BOXES; ++b) {
        unsigned int mask = 0;
        int br = (b / 2) * SudokuBoard::BOX_ROWS;
        int bc = (b % 2) * SudokuBoard::BOX_COLS;
        for (int dr = 0; dr < SudokuBoard::BOX_ROWS; ++dr) {
            for (int dc = 0; dc < SudokuBoard::BOX_COLS; ++dc) {
                int d = solution[br + dr][bc + dc];
                if (mask & (1u << d)) {
                    return {false, "Box " + std::to_string(b + 1) + " (2x3) contains duplicate digit " + std::to_string(d)};
                }
                mask |= (1u << d);
            }
        }
        if (mask != 0b1111110u) {
            return {false, "Box " + std::to_string(b + 1) + " does not contain all digits 1-6"};
        }
    }

    return {true, ""};
}

} // namespace solvedin
