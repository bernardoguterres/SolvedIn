#pragma once

#include "solvedin/common.hpp"
#include "solvedin/patches_board.hpp"

#include <string>
#include <vector>

namespace solvedin {

struct Patch {
    Position clue_pos{-1, -1};
    std::vector<Position> cells;

    bool operator==(const Patch& other) const = default;
};

struct PatchesSolution {
    std::vector<Patch> patches;

    bool operator==(const PatchesSolution& other) const = default;

    [[nodiscard]] std::vector<std::string> to_char_grid(int rows, int cols) const;
    [[nodiscard]] std::string to_string(int rows, int cols) const;
};

/// Independently validates a Patches solution against the board's rules and clues:
/// 1. Exactly complete coverage of the board (every cell belongs to exactly one patch).
/// 2. Exactly one clue per patch.
/// 3. Every clue on the board is represented in exactly one patch.
/// 4. Each patch is 4-connected (orthogonally connected).
/// 5. Each patch obeys the clue's size constraint (if specified).
/// 6. Each patch obeys the clue's shape constraint (Square, Tall, Wide, Freeform) if specified.
ValidationResult validate_patches_solution(
    const PatchesBoard& board,
    const PatchesSolution& solution);

} // namespace solvedin
