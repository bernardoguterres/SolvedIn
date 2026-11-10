#include "solvedin/board.hpp"
#include "solvedin/queens_validator.hpp"

#include <cassert>
#include <iostream>
#include <vector>

void test_valid_placement() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_8x8_unique.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Known solution: [0, 3, 6, 2, 5, 1, 4, 7]
    std::vector<solvedin::Position> queens = {
        {0, 0}, {1, 3}, {2, 6}, {3, 2},
        {4, 5}, {5, 1}, {6, 4}, {7, 7}
    };

    auto res = solvedin::validate_queens_solution(board, queens);
    assert(res.is_valid);
    assert(res.error_message == "Valid");
    std::cout << "[PASS] test_valid_placement\n";
}

void test_legal_placement_sharing_longer_diagonal() {
    // In LinkedIn Queens, queens CAN share a diagonal if |dr| > 1 && |dc| > 1.
    // For example, (0, 0) and (7, 7) share the main diagonal across 8x8 (dr=7, dc=7).
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_8x8_unique.txt");
    assert(success);
    const auto& board = *maybe_board;

    std::vector<solvedin::Position> queens = {
        {0, 0}, {1, 3}, {2, 6}, {3, 2},
        {4, 5}, {5, 1}, {6, 4}, {7, 7}
    };

    // Explicitly verify diagonal distance between (0, 0) and (7, 7)
    int dr = std::abs(queens[0].row - queens[7].row);
    int dc = std::abs(queens[0].col - queens[7].col);
    assert(dr == dc && dr == 7); // Long diagonal!
    (void)dr;
    (void)dc;

    auto res = solvedin::validate_queens_solution(board, queens);
    assert(res.is_valid); // Must be accepted!
    std::cout << "[PASS] test_legal_placement_sharing_longer_diagonal\n";
}

void test_illegal_placement_diagonally_touching() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Placements with (1, 2) and (2, 1) diagonally touching!
    // (0,0) is region 1, (1,2) is region 2, (2,1) is region 3, (3,3) is region 4
    // Rows and columns and regions are distinct, but (1,2) and (2,1) touch diagonally (|dr|=1, |dc|=1).
    std::vector<solvedin::Position> touching_queens = {
        {0, 0}, {1, 2}, {2, 1}, {3, 3}
    };

    auto res = solvedin::validate_queens_solution(board, touching_queens);
    assert(!res.is_valid);
    assert(res.error_message.find("touch diagonally") != std::string::npos);
    std::cout << "[PASS] test_illegal_placement_diagonally_touching\n";
}

void test_illegal_row_conflict() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Two queens in row 0: (0, 1) and (0, 3)
    std::vector<solvedin::Position> queens = {
        {0, 1}, {0, 3}, {2, 0}, {3, 2}
    };

    auto res = solvedin::validate_queens_solution(board, queens);
    assert(!res.is_valid);
    assert(res.error_message.find("Row conflict") != std::string::npos);
    std::cout << "[PASS] test_illegal_row_conflict\n";
}

void test_illegal_col_conflict() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    // Two queens in column 1: (0, 1) and (3, 1)
    std::vector<solvedin::Position> queens = {
        {0, 1}, {1, 3}, {2, 0}, {3, 1}
    };

    auto res = solvedin::validate_queens_solution(board, queens);
    assert(!res.is_valid);
    assert(res.error_message.find("Column conflict") != std::string::npos);
    std::cout << "[PASS] test_illegal_col_conflict\n";
}

void test_illegal_region_conflict() {
    // Board where region 1 is at (0, 0) and (3, 3) (far apart, so no touching or row/col conflict)
    std::string b_str = "4\n1 2 2 1\n3 3 4 4\n3 3 4 4\n1 2 2 1\n";
    auto b_res = solvedin::Board::parse_from_string(b_str);
    assert(b_res.success);
    const auto& b = *b_res.board;

    // Both queens in region 1
    std::vector<solvedin::Position> queens = {
        {0, 0}, {1, 2}, {2, 1}, {3, 3}
    };

    auto res = solvedin::validate_queens_solution(b, queens);
    assert(!res.is_valid);
    assert(res.error_message.find("Region conflict") != std::string::npos);
    std::cout << "[PASS] test_illegal_region_conflict\n";
}

void test_illegal_queen_count() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    std::vector<solvedin::Position> too_few = {
        {0, 1}, {1, 3}, {2, 0}
    };
    auto res_few = solvedin::validate_queens_solution(board, too_few);
    assert(!res_few.is_valid);
    assert(res_few.error_message.find("Invalid queen count") != std::string::npos);

    std::vector<solvedin::Position> too_many = {
        {0, 1}, {1, 3}, {2, 0}, {3, 2}, {1, 1}
    };
    auto res_many = solvedin::validate_queens_solution(board, too_many);
    assert(!res_many.is_valid);
    assert(res_many.error_message.find("Invalid queen count") != std::string::npos);

    std::cout << "[PASS] test_illegal_queen_count\n";
}

void test_out_of_bounds() {
    auto [success, maybe_board, err] = solvedin::Board::load_from_file("fixtures/valid_4x4_multiple.txt");
    assert(success);
    const auto& board = *maybe_board;

    std::vector<solvedin::Position> oob = {
        {0, 1}, {1, 3}, {2, 0}, {4, 2} // row 4 is out of bounds for N=4
    };
    auto res = solvedin::validate_queens_solution(board, oob);
    assert(!res.is_valid);
    assert(res.error_message.find("outside board boundaries") != std::string::npos);
    std::cout << "[PASS] test_out_of_bounds\n";
}

int main() {
    std::cout << "Running Validator Tests...\n";
    test_valid_placement();
    test_legal_placement_sharing_longer_diagonal();
    test_illegal_placement_diagonally_touching();
    test_illegal_row_conflict();
    test_illegal_col_conflict();
    test_illegal_region_conflict();
    test_illegal_queen_count();
    test_out_of_bounds();
    std::cout << "All Validator Tests Passed!\n";
    return 0;
}
