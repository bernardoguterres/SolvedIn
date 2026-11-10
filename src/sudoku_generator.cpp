#include "solvedin/sudoku_generator.hpp"
#include "solvedin/sudoku_solver.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <sstream>
#include <utility>
#include <vector>

namespace solvedin {

namespace {

constexpr int N = SudokuBoard::SIZE;

std::vector<int> get_cell_candidates(
    int r, int c,
    const std::array<uint8_t, N>& row_mask,
    const std::array<uint8_t, N>& col_mask,
    const std::array<uint8_t, N>& box_mask) {

    int b = SudokuBoard::box_index(r, c);
    uint8_t used = row_mask[r] | col_mask[c] | box_mask[b];
    std::vector<int> candidates;
    candidates.reserve(6);
    for (int d = 1; d <= 6; ++d) {
        if (!(used & (1 << d))) {
            candidates.push_back(d);
        }
    }
    return candidates;
}

bool fill_random_sudoku(
    int r, int c,
    std::array<std::array<int, N>, N>& grid,
    std::array<uint8_t, N>& row_mask,
    std::array<uint8_t, N>& col_mask,
    std::array<uint8_t, N>& box_mask,
    std::mt19937_64& rng) {

    if (r == N) {
        return true;
    }

    int next_r = (c + 1 == N) ? r + 1 : r;
    int next_c = (c + 1 == N) ? 0 : c + 1;

    auto cands = get_cell_candidates(r, c, row_mask, col_mask, box_mask);
    std::shuffle(cands.begin(), cands.end(), rng);

    int b = SudokuBoard::box_index(r, c);
    for (int d : cands) {
        grid[r][c] = d;
        row_mask[r] |= static_cast<uint8_t>(1 << d);
        col_mask[c] |= static_cast<uint8_t>(1 << d);
        box_mask[b] |= static_cast<uint8_t>(1 << d);

        if (fill_random_sudoku(next_r, next_c, grid, row_mask, col_mask, box_mask, rng)) {
            return true;
        }

        row_mask[r] &= static_cast<uint8_t>(~(1 << d));
        col_mask[c] &= static_cast<uint8_t>(~(1 << d));
        box_mask[b] &= static_cast<uint8_t>(~(1 << d));
        grid[r][c] = 0;
    }

    return false;
}

} // namespace

SudokuBoard SudokuGenerator::generate(uint64_t seed, int remove_count) {
    std::mt19937_64 rng(seed);

    // 1. Generate a random valid full 6x6 Mini Sudoku grid
    std::array<std::array<int, N>, N> grid{};
    std::array<uint8_t, N> row_mask{};
    std::array<uint8_t, N> col_mask{};
    std::array<uint8_t, N> box_mask{};

    fill_random_sudoku(0, 0, grid, row_mask, col_mask, box_mask, rng);

    // 2. Carve out cells while verifying uniqueness with SudokuSolver::solve
    std::vector<std::pair<int, int>> cells;
    cells.reserve(N * N);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            cells.push_back({r, c});
        }
    }
    std::shuffle(cells.begin(), cells.end(), rng);

    int target_removals = std::clamp(remove_count, 0, N * N);
    int removed = 0;

    for (const auto& [r, c] : cells) {
        if (removed >= target_removals) {
            break;
        }

        int original_val = grid[r][c];
        grid[r][c] = 0;

        SudokuBoard candidate_board(grid);
        auto result = solve_sudoku(candidate_board, true); // check_unique = true
        if (result.status == SolveStatus::UNIQUE_SOLUTION) {
            removed++;
        } else {
            // Uniqueness lost; restore cell
            grid[r][c] = original_val;
        }
    }

    return SudokuBoard(grid);
}

std::string SudokuGenerator::format_puzzle(const SudokuBoard& board) {
    std::ostringstream ss;
    ss << SudokuBoard::SIZE << "\n";
    for (int r = 0; r < SudokuBoard::SIZE; ++r) {
        for (int c = 0; c < SudokuBoard::SIZE; ++c) {
            int val = board.get(r, c);
            if (val == 0) {
                ss << ".";
            } else {
                ss << val;
            }
            if (c + 1 < SudokuBoard::SIZE) {
                ss << " ";
            }
        }
        ss << "\n";
    }
    return ss.str();
}

SudokuBoard generate_sudoku(uint64_t seed, int remove_count) {
    return SudokuGenerator::generate(seed, remove_count);
}

std::string format_sudoku(const SudokuBoard& board) {
    return SudokuGenerator::format_puzzle(board);
}

} // namespace solvedin
