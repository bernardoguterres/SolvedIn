#include "solvedin/board.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"

#include <cassert>
#include <iostream>

void test_solve_8x8_unique() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_8x8_unique.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Ordinary solve mode: returns first solution
    auto single_res = solvedin::solve_queens(board, false);
    assert(single_res.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    assert(single_res.solutions.size() == 1);
    auto val1 = solvedin::validate_queens_solution(board, single_res.solutions[0].queens);
    assert(val1.is_valid);

    // Uniqueness mode: checks unique
    auto unique_res = solvedin::solve_queens(board, true);
    assert(unique_res.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    assert(unique_res.solutions.size() == 1);
    auto val2 = solvedin::validate_queens_solution(board, unique_res.solutions[0].queens);
    assert(val2.is_valid);

    std::cout << "[PASS] test_solve_8x8_unique\n";
}

void test_solve_4x4_multiple() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Ordinary solve mode: returns first solution
    auto single_res = solvedin::solve_queens(board, false);
    assert(single_res.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    assert(single_res.solutions.size() == 1);
    auto val1 = solvedin::validate_queens_solution(board, single_res.solutions[0].queens);
    assert(val1.is_valid);

    // Uniqueness mode: stops after 2 solutions and reports MULTIPLE_SOLUTIONS
    auto multi_res = solvedin::solve_queens(board, true);
    assert(multi_res.status == solvedin::SolveStatus::MULTIPLE_SOLUTIONS);
    assert(multi_res.solutions.size() == 2);

    auto val_sol1 = solvedin::validate_queens_solution(board, multi_res.solutions[0].queens);
    auto val_sol2 = solvedin::validate_queens_solution(board, multi_res.solutions[1].queens);
    assert(val_sol1.is_valid);
    assert(val_sol2.is_valid);

    // Ensure the two solutions are distinct
    assert(multi_res.solutions[0].queens != multi_res.solutions[1].queens);

    std::cout << "[PASS] test_solve_4x4_multiple\n";
}

void test_solve_no_solution() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_no_solution.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Ordinary mode
    auto single_res = solvedin::solve_queens(board, false);
    assert(single_res.status == solvedin::SolveStatus::NO_SOLUTION);
    assert(single_res.solutions.empty());

    // Uniqueness mode
    auto unique_res = solvedin::solve_queens(board, true);
    assert(unique_res.status == solvedin::SolveStatus::NO_SOLUTION);
    assert(unique_res.solutions.empty());

    std::cout << "[PASS] test_solve_no_solution\n";
}

void test_solve_long_diagonal() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_long_diagonal.txt");
    assert(success);
    const auto& board = *maybe_board;

    auto res = solvedin::solve_queens(board, true);
    assert(res.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    assert(res.solutions.size() == 1);

    const auto& queens = res.solutions[0].queens;
    auto val = solvedin::validate_queens_solution(board, queens);
    assert(val.is_valid);

    // Check queens at row 0 and row 4 share a diagonal
    assert(queens[0].row == 0 && queens[0].col == 0);
    assert(queens[4].row == 4 && queens[4].col == 4);
    assert(std::abs(queens[0].row - queens[4].row) == 4);
    assert(std::abs(queens[0].col - queens[4].col) == 4);

    std::cout << "[PASS] test_solve_long_diagonal\n";
}

int main() {
    std::cout << "Running Solver Tests...\n";
    test_solve_8x8_unique();
    test_solve_4x4_multiple();
    test_solve_no_solution();
    test_solve_long_diagonal();
    std::cout << "All Solver Tests Passed!\n";
    return 0;
}
