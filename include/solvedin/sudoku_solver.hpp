#pragma once

#include "solvedin/common.hpp"
#include "solvedin/sudoku_board.hpp"

#include <array>
#include <vector>

namespace solvedin {

struct SudokuSolution {
    std::array<std::array<int, SudokuBoard::SIZE>, SudokuBoard::SIZE> grid{};

    bool operator==(const SudokuSolution& other) const = default;
};

struct SudokuSolveResult {
    SolveStatus status{SolveStatus::NO_SOLUTION};
    std::vector<SudokuSolution> solutions;
    SolveStats stats;
};

class SudokuSolver {
public:
    explicit SudokuSolver(const SudokuBoard& board, SolveOptions options = {});

    void set_options(SolveOptions options) noexcept { options_ = options; }
    SudokuSolveResult solve(bool check_unique = false);

private:
    const SudokuBoard& board_;
    SolveOptions options_;
};

SudokuSolveResult solve_sudoku(const SudokuBoard& board, bool check_unique = false, SolveOptions options = {});

} // namespace solvedin
