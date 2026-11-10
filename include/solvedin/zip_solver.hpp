#pragma once

#include "solvedin/common.hpp"
#include "solvedin/zip_board.hpp"

#include <string>
#include <vector>

namespace solvedin {

struct ZipSolution {
    std::vector<Position> path;

    bool operator==(const ZipSolution& other) const = default;

    [[nodiscard]] std::vector<std::vector<int>> to_grid(int rows, int cols) const;
    [[nodiscard]] std::string to_string(int rows, int cols) const;
};

struct ZipSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<ZipSolution> solutions;
    SolveStats stats;
};

class ZipSolver {
public:
    explicit ZipSolver(const ZipBoard& board, SolveOptions options = {});

    void set_options(SolveOptions options) noexcept { options_ = options; }
    [[nodiscard]] ZipSolveResult solve(bool check_unique = false);

private:
    const ZipBoard& board_;
    SolveOptions options_;
};

ZipSolveResult solve_zip(const ZipBoard& board, bool check_unique = false, SolveOptions options = {});

} // namespace solvedin
