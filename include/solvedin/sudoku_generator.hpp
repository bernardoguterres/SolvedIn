#pragma once

#include "solvedin/sudoku_board.hpp"

#include <cstdint>
#include <string>

namespace solvedin {

class SudokuGenerator {
public:
    /// Generates a 6x6 Mini Sudoku grid with a unique solution for a given random seed.
    /// Starts from a random solved board and carves out cells while verifying uniqueness.
    static SudokuBoard generate(uint64_t seed, int remove_count = 20);

    /// Formats a SudokuBoard into standard puzzle input format (readable by parse_from_string).
    static std::string format_puzzle(const SudokuBoard& board);
};

/// Standalone generator function matching the requested signature:
/// SudokuBoard generate_sudoku(uint64_t seed, int remove_count = 20);
SudokuBoard generate_sudoku(uint64_t seed, int remove_count = 20);

/// Helper function to format a SudokuBoard into standard puzzle input format.
std::string format_sudoku(const SudokuBoard& board);

} // namespace solvedin
