#pragma once

#include "solvedin/board.hpp"
#include "solvedin/common.hpp"

#include <string>
#include <vector>

namespace solvedin {

/// Independently validates queen placements against board rules:
/// 1. Exactly N queens placed.
/// 2. All coordinates are within [0, N-1].
/// 3. Exactly one queen per row.
/// 4. Exactly one queen per column.
/// 5. Exactly one queen per region.
/// 6. No two queens touch horizontally, vertically, or diagonally (|dr| <= 1 && |dc| <= 1).
/// Note: Queens CAN share longer diagonals (|dr| == |dc| > 1).
ValidationResult validate_queens_solution(const Board& board, const std::vector<Position>& queens);

/// Convenience overload accepting column index per row (where queens[r] is the col for row r).
ValidationResult validate_queens_solution(const Board& board, const std::vector<int>& queen_cols);

} // namespace solvedin
