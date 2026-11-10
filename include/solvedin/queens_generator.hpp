#pragma once

#include "solvedin/board.hpp"

#include <cstdint>
#include <vector>

namespace solvedin {

struct GeneratedBoard {
    Board board;
    std::vector<Position> planted_queens;
};

class QueensGenerator {
public:
    /// Generates a valid LinkedIn Queens board of size N using PRNG seed.
    /// 1. Generates a valid non-touching queen placement.
    /// 2. Grows N connected, non-overlapping regions from the queen cells until the board is filled.
    /// 3. Independently verifies the planted placement and region connectivity.
    static GeneratedBoard generate(int size, uint64_t seed);

    /// Independently verifies that each of the N regions in board is 4-connected
    /// and contains exactly its planted queen.
    static bool verify_region_connectivity(const Board& board, const std::vector<Position>& planted_queens);
};

} // namespace solvedin
