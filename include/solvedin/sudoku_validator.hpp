#pragma once

#include "solvedin/common.hpp"
#include "solvedin/sudoku_board.hpp"

#include <array>

namespace solvedin {

/// Independently validates a 6x6 Mini Sudoku solution against the initial board rules:
/// 1. All cells are filled with digits 1..6.
/// 2. All initial givens are strictly preserved.
/// 3. Each row contains digits 1..6 exactly once.
/// 4. Each column contains digits 1..6 exactly once.
/// 5. Each 2-row by 3-column box contains digits 1..6 exactly once.
ValidationResult validate_sudoku_solution(
    const SudokuBoard& initial,
    const std::array<std::array<int, SudokuBoard::SIZE>, SudokuBoard::SIZE>& solution);

} // namespace solvedin
