#pragma once

#include "solvedin/common.hpp"
#include "solvedin/tango_board.hpp"

#include <string>
#include <vector>

namespace solvedin {

/// Independently validates a Tango solution against the board's rules and clues:
/// 1. Solution preserves all original given symbols.
/// 2. Every cell contains either 'S' or 'M'.
/// 3. Equal counts of 'S' and 'M' in every row and every column (N/2 each).
/// 4. No run of three identical symbols horizontally or vertically.
/// 5. All horizontal edge clues (= for same, x for opposite) are satisfied.
/// 6. All vertical edge clues (= for same, x for opposite) are satisfied.
ValidationResult validate_tango_solution(
    const TangoBoard& initial,
    const std::vector<std::string>& solution);

} // namespace solvedin
