#pragma once

#include "solvedin/common.hpp"
#include "solvedin/patches_board.hpp"
#include "solvedin/patches_validator.hpp"

#include <vector>

namespace solvedin {

struct PatchesSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<PatchesSolution> solutions;
    SolveStats stats;
};

class PatchesSolver {
public:
    explicit PatchesSolver(const PatchesBoard& board, SolveOptions options = {});

    void set_options(SolveOptions options) noexcept { options_ = options; }
    [[nodiscard]] PatchesSolveResult solve(bool check_unique = false);

private:
    const PatchesBoard& board_;
    SolveOptions options_;
};

PatchesSolveResult solve_patches(const PatchesBoard& board, bool check_unique = false, SolveOptions options = {});

} // namespace solvedin
