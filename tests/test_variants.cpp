#include "solvedin/board.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_board_variants(const std::string& path,
                         solvedin::SolveStatus expected_status,
                         size_t expected_sol_count) {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file(path);
    CHECK_ASSERT(success);
    const auto& board = *maybe_board;

    // Run Baseline
    auto res_base = solvedin::solve_queens(board, true, solvedin::SolverVariant::BASELINE);
    // Run MRV
    auto res_mrv = solvedin::solve_queens(board, true, solvedin::SolverVariant::MRV);
    // Run Bitset
    auto res_bit = solvedin::solve_queens(board, true, solvedin::SolverVariant::BITSET);

    // 1. Status equivalence
    CHECK_ASSERT(res_base.status == expected_status);
    CHECK_ASSERT(res_mrv.status == expected_status);
    CHECK_ASSERT(res_bit.status == expected_status);

    // 2. Solution counts
    CHECK_ASSERT(res_base.solutions.size() == expected_sol_count);
    CHECK_ASSERT(res_mrv.solutions.size() == expected_sol_count);
    CHECK_ASSERT(res_bit.solutions.size() == expected_sol_count);

    // 3. Search order & node count equivalence between MRV and Bitset:
    // Because Bitset implements the exact same search heuristic and tie-breaking as MRV,
    // their search trees and node counts must be identical!
    CHECK_ASSERT(res_mrv.stats.nodes_visited == res_bit.stats.nodes_visited);

    // 4. Validate every returned solution independently
    for (const auto& sol : res_base.solutions) {
        CHECK_ASSERT(solvedin::validate_queens_solution(board, sol.queens).is_valid);
    }
    for (const auto& sol : res_mrv.solutions) {
        CHECK_ASSERT(solvedin::validate_queens_solution(board, sol.queens).is_valid);
    }
    for (const auto& sol : res_bit.solutions) {
        CHECK_ASSERT(solvedin::validate_queens_solution(board, sol.queens).is_valid);
    }

    std::cout << "[PASS] Variants test for " << path
              << " | Nodes: Baseline=" << res_base.stats.nodes_visited
              << ", MRV=" << res_mrv.stats.nodes_visited
              << ", Bitset=" << res_bit.stats.nodes_visited
              << " | MRV == Bitset nodes: YES\n";
}

int main() {
    std::cout << "Running Solver Variant Cross-Verification Tests...\n";

    test_board_variants("fixtures/valid_8x8_unique.txt",
                        solvedin::SolveStatus::UNIQUE_SOLUTION, 1);

    test_board_variants("fixtures/valid_4x4_multiple.txt",
                        solvedin::SolveStatus::MULTIPLE_SOLUTIONS, 2);

    test_board_variants("fixtures/valid_no_solution.txt",
                        solvedin::SolveStatus::NO_SOLUTION, 0);

    test_board_variants("fixtures/valid_long_diagonal.txt",
                        solvedin::SolveStatus::UNIQUE_SOLUTION, 1);

    std::cout << "All Solver Variant Cross-Verification Tests Passed!\n";
    return 0;
}
