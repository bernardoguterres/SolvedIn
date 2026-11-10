#pragma once

// Explainable Queens solver: one queen per row, column and region; queens may not touch, including diagonally.
// Deductions (cheapest first): queen attacks, lone candidates, region<->line locks in both directions,
// eliminations that would empty another group, and small k-group locks; then a fallback that branches on the
// group with the fewest candidates and re-applies the deductions in every branch.

#include "solvedin/board.hpp"
#include "solvedin/reasoning.hpp"

#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace queens_logic {

enum class Cell : char { OPEN = 'o', QUEEN = 'Q', OUT = 'x' };

struct State {
    int n{0};
    std::vector<Cell> cells;  // row-major
    std::vector<std::vector<int>> region_cells;  // cell indices of each region (by region index), fixed
    [[nodiscard]] Cell at(int r, int c) const { return cells[static_cast<size_t>(r * n + c)]; }
};

// Group locks (k = 2, 3) are only tried up to these sizes, so very large boards stay fast.
constexpr int kMaxLockBoard2 = 32;
constexpr int kMaxLockBoard3 = 16;
// The "would empty another group" rule looks at groups with at most this many candidates.
constexpr int kMaxEmptyGroupCandidates = 6;

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<Position> queens;      // placements
    std::vector<Position> eliminated;  // candidates removed
    bool contradiction{false};
};

[[nodiscard]] State initial_state(const Board& board);
[[nodiscard]] std::optional<Deduction> next_deduction(const Board& board, const State& s);
void apply(State& s, const Deduction& d);
[[nodiscard]] std::string render(const Board& board, const State& s);

} // namespace queens_logic

[[nodiscard]] ReasoningReport reason_queens(const Board& board, const ReasoningOptions& options = {});

} // namespace solvedin
