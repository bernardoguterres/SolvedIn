#pragma once

#include "solvedin/common.hpp"
#include "solvedin/tango_board.hpp"

#include <string>
#include <vector>

namespace solvedin {

struct TangoSolution {
    std::vector<std::string> grid;

    bool operator==(const TangoSolution& other) const = default;
};

struct TangoSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<TangoSolution> solutions;
    SolveStats stats;
};

class TangoSolver {
public:
    explicit TangoSolver(const TangoBoard& board, SolveOptions options = {});

    void set_options(SolveOptions options) noexcept { options_ = options; }
    TangoSolveResult solve(bool check_unique = false);

private:
    const TangoBoard& board_;
    SolveOptions options_;
};

TangoSolveResult solve_tango(const TangoBoard& board, bool check_unique = false, SolveOptions options = {});

} // namespace solvedin
