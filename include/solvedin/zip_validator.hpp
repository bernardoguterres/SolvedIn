#pragma once

#include "solvedin/common.hpp"
#include "solvedin/zip_board.hpp"

#include <vector>

namespace solvedin {

/// Independently validates a complete Zip path against the board's rules:
/// 1. Path visits every cell on the board exactly once.
/// 2. Path length equals the total number of cells (rows * cols).
/// 3. Path starts at checkpoint 1 and ends at checkpoint K.
/// 4. Every step is an orthogonal move between adjacent cells.
/// 5. No step crosses a horizontal or vertical wall.
/// 6. Checkpoints 1..K appear along the path in strictly increasing sequential order.
ValidationResult validate_zip_solution(
    const ZipBoard& board,
    const std::vector<Position>& path);

} // namespace solvedin
