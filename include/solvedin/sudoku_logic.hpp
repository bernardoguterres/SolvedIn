#pragma once

// Explainable Mini Sudoku (6x6, boxes of 2 rows x 3 columns) on candidate masks.
// Rules: naked singles, hidden singles, locked candidates (box -> line and line -> box) and naked pairs;
// then a fallback that branches on the cell with the fewest candidates and propagates again.

#include "solvedin/reasoning.hpp"
#include "solvedin/sudoku_board.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace solvedin {

namespace sudoku_logic {

constexpr int N = SudokuBoard::SIZE;

struct State {
    std::array<std::array<int, N>, N> grid{};        // 0 = open
    std::array<std::array<uint8_t, N>, N> removed{};  // digit bits (1 << d) eliminated by earlier deductions
};

struct Deduction {
    std::string rule;
    std::string explanation;
    std::vector<Position> involved;
    std::vector<std::pair<Position, int>> placements;   // cell -> digit
    std::vector<std::pair<Position, int>> eliminations; // cell -> digit removed
    bool contradiction{false};
};

[[nodiscard]] State initial_state(const SudokuBoard& board);
[[nodiscard]] uint8_t candidates(const State& s, int r, int c);  // bit d set when digit d still fits
// basic = singles only.
[[nodiscard]] std::optional<Deduction> next_deduction(const State& s, bool basic = false);
void apply(State& s, const Deduction& d);
[[nodiscard]] std::string render(const State& s);

} // namespace sudoku_logic

[[nodiscard]] ReasoningReport reason_sudoku(const SudokuBoard& board, const ReasoningOptions& options = {});

} // namespace solvedin
