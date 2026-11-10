#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_solver.hpp"
#include "solvedin/sudoku_validator.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

#define CHECK_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "Assertion failed at " << __FILE__ << ":" << __LINE__ << ": " #cond << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_sudoku_box_indexing() {
    // 6x6 grid with 2 rows x 3 cols per box
    // Box 0: r in [0,1], c in [0,2]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(0, 0) == 0);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(0, 2) == 0);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(1, 2) == 0);

    // Box 1: r in [0,1], c in [3,5]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(0, 3) == 1);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(1, 5) == 1);

    // Box 2: r in [2,3], c in [0,2]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(2, 0) == 2);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(3, 2) == 2);

    // Box 3: r in [2,3], c in [3,5]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(2, 3) == 3);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(3, 5) == 3);

    // Box 4: r in [4,5], c in [0,2]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(4, 0) == 4);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(5, 2) == 4);

    // Box 5: r in [4,5], c in [3,5]
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(4, 3) == 5);
    CHECK_ASSERT(solvedin::SudokuBoard::box_index(5, 5) == 5);

    std::cout << "[PASS] Sudoku box indexing\n";
}

void test_sudoku_load_and_solve_unique() {
    auto [success, maybe_board, err] = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_valid_unique.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());

    const auto& board = *maybe_board;
    CHECK_ASSERT(board.givens_count() == 12);

    auto result = solvedin::solve_sudoku(board, true);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(result.solutions.size() == 1);

    auto val = solvedin::validate_sudoku_solution(board, result.solutions[0].grid);
    CHECK_ASSERT(val.is_valid);
    CHECK_ASSERT(val.error_message.empty());

    // Check specific known cell in solved grid
    CHECK_ASSERT(result.solutions[0].grid[0][0] == 1);
    CHECK_ASSERT(result.solutions[0].grid[5][5] == 2);

    std::cout << "[PASS] Sudoku load and solve unique\n";
}

void test_sudoku_branching() {
    // Tests a puzzle where propagation (naked + hidden singles) stalls,
    // demonstrably forcing the solver to branch into the search tree.
    auto [success, maybe_board, err] = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_branching.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());

    const auto& board = *maybe_board;
    auto result = solvedin::solve_sudoku(board, true);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(result.solutions.size() == 1);

    // Demonstrates that search tree branching occurred (> 1 node visited)
    CHECK_ASSERT(result.stats.nodes_visited > 1);
    CHECK_ASSERT(result.stats.nodes_visited == 4);

    // Independently validate the returned solution
    auto val = solvedin::validate_sudoku_solution(board, result.solutions[0].grid);
    CHECK_ASSERT(val.is_valid);
    CHECK_ASSERT(val.error_message.empty());

    std::cout << "[PASS] Sudoku branching after propagation (nodes: " << result.stats.nodes_visited << ")\n";
}

void test_sudoku_solve_multiple() {
    auto [success, maybe_board, err] = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_valid_multiple.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());

    const auto& board = *maybe_board;
    auto result = solvedin::solve_sudoku(board, true);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::MULTIPLE_SOLUTIONS);
    CHECK_ASSERT(result.solutions.size() >= 2);

    for (const auto& sol : result.solutions) {
        auto val = solvedin::validate_sudoku_solution(board, sol.grid);
        CHECK_ASSERT(val.is_valid);
    }

    std::cout << "[PASS] Sudoku solve multiple solutions\n";
}

void test_sudoku_contradictory_and_malformed() {
    // Contradictory givens
    auto res_contra = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_contradictory.txt");
    CHECK_ASSERT(!res_contra.success);
    CHECK_ASSERT(res_contra.error_message.find("Contradictory givens") != std::string::npos);
    std::cout << "[PASS] Sudoku contradictory givens rejected: " << res_contra.error_message << "\n";

    // Malformed dimensions
    auto res_malformed = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_malformed_dimensions.txt");
    CHECK_ASSERT(!res_malformed.success);
    CHECK_ASSERT(res_malformed.error_message.find("Malformed sudoku dimensions") != std::string::npos);
    std::cout << "[PASS] Sudoku malformed dimensions rejected: " << res_malformed.error_message << "\n";
}

void test_sudoku_validator_failures() {
    auto [success, maybe_board, err] = solvedin::SudokuBoard::load_from_file("fixtures/sudoku_valid_unique.txt");
    CHECK_ASSERT(success);
    const auto& board = *maybe_board;

    auto result = solvedin::solve_sudoku(board, false);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    auto grid = result.solutions[0].grid;

    // 1. Alter given
    auto corrupted_given = grid;
    corrupted_given[0][1] = 6; // was given 2
    auto val1 = solvedin::validate_sudoku_solution(board, corrupted_given);
    CHECK_ASSERT(!val1.is_valid);
    CHECK_ASSERT(val1.error_message.find("Given digit") != std::string::npos);

    // 2. Duplicate in row
    auto corrupted_row = grid;
    corrupted_row[0][0] = corrupted_row[0][5];
    auto val2 = solvedin::validate_sudoku_solution(board, corrupted_row);
    CHECK_ASSERT(!val2.is_valid);

    // 3. Duplicate in box
    auto corrupted_box = grid;
    corrupted_box[0][0] = corrupted_box[1][1];
    auto val3 = solvedin::validate_sudoku_solution(board, corrupted_box);
    CHECK_ASSERT(!val3.is_valid);

    std::cout << "[PASS] Sudoku validator failure detections\n";
}

int main() {
    std::cout << "Running Mini Sudoku Tests...\n";
    test_sudoku_box_indexing();
    test_sudoku_load_and_solve_unique();
    test_sudoku_branching();
    test_sudoku_solve_multiple();
    test_sudoku_contradictory_and_malformed();
    test_sudoku_validator_failures();
    std::cout << "All Mini Sudoku Tests Passed!\n";
    return 0;
}
