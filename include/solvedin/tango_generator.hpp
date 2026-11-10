#pragma once

#include "solvedin/tango_board.hpp"

#include <cstdint>

namespace solvedin {

class TangoGenerator {
public:
    /// Generates a valid 6x6 Tango puzzle with givens and edge clues (=, x)
    /// that has a unique solution for a given seed.
    static TangoBoard generate(uint64_t seed, int given_count = 8, int edge_clue_count = 6);
};

/// Standalone generator function matching the requested signature:
/// TangoBoard generate_tango(uint64_t seed, int given_count = 8, int edge_clue_count = 6);
TangoBoard generate_tango(uint64_t seed, int given_count = 8, int edge_clue_count = 6);

} // namespace solvedin
