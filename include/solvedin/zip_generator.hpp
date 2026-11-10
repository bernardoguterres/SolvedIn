#pragma once

#include "solvedin/zip_board.hpp"

#include <cstdint>

namespace solvedin {

class ZipGenerator {
public:
    /// Generates a random valid Hamiltonian walk on an RxC board, places checkpoint numbers in order,
    /// and optionally adds walls while ensuring a unique solution.
    static ZipBoard generate(int rows, int cols, int num_checkpoints, uint64_t seed);
};

/// Standalone generator function matching the requested signature:
/// ZipBoard generate_zip(int rows, int cols, int num_checkpoints, uint64_t seed);
ZipBoard generate_zip(int rows, int cols, int num_checkpoints, uint64_t seed);

} // namespace solvedin
