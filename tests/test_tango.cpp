#include "solvedin/tango_board.hpp"
#include "solvedin/tango_solver.hpp"
#include "solvedin/tango_validator.hpp"

#include <cstdlib>
#include <iostream>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_tango_load_and_solve_unique() {
    auto [success, maybe_board, err] = solvedin::TangoBoard::load_from_file("fixtures/tango_valid_unique.txt");
    CHECK_ASSERT(success);
    CHECK_ASSERT(maybe_board.has_value());

    const auto& board = *maybe_board;
    CHECK_ASSERT(board.size() == 6);
    CHECK_ASSERT(board.get_symbol(0, 0) == solvedin::TangoSymbol::SUN);
    CHECK_ASSERT(board.get_horizontal_clue(3, 1) == solvedin::EdgeClue::SAME);

    auto result = solvedin::solve_tango(board, true);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(result.solutions.size() == 1);

    auto val = solvedin::validate_tango_solution(board, result.solutions[0].grid);
    CHECK_ASSERT(val.is_valid);
    CHECK_ASSERT(val.error_message.empty());

    // Expected solution row 0: "SMSMMS"
    CHECK_ASSERT(result.solutions[0].grid[0] == "SMSMMS");
    CHECK_ASSERT(result.solutions[0].grid[3] == "MSSMSM");

    std::cout << "[PASS] Tango load and solve unique\n";
}

void test_tango_edge_clue_dependency() {
    // 1. With key horizontal clue (3, 1) '=': Unique solution
    auto [s1, b1, err1] = solvedin::TangoBoard::load_from_file("fixtures/tango_edge_clue_dependent.txt");
    CHECK_ASSERT(s1);
    CHECK_ASSERT(b1->get_horizontal_clue(3, 1) == solvedin::EdgeClue::SAME);

    auto res1 = solvedin::solve_tango(*b1, true);
    CHECK_ASSERT(res1.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(res1.solutions.size() == 1);
    const auto& sol1 = res1.solutions[0].grid;
    CHECK_ASSERT(sol1[3][1] == sol1[3][2]); // Clue satisfied: S == S

    // 2. Without key horizontal clue (3, 1) '.': Multiple solutions exist, producing a different answer
    auto [s2, b2, err2] = solvedin::TangoBoard::load_from_file("fixtures/tango_edge_clue_ignored.txt");
    CHECK_ASSERT(s2);
    CHECK_ASSERT(b2->get_horizontal_clue(3, 1) == solvedin::EdgeClue::NONE);

    auto res2 = solvedin::solve_tango(*b2, true);
    CHECK_ASSERT(res2.status == solvedin::SolveStatus::MULTIPLE_SOLUTIONS);
    CHECK_ASSERT(res2.solutions.size() >= 2);

    // In the alternative solutions without the edge clue, at least one violates sol1[3][1] == sol1[3][2]
    bool found_different_answer = false;
    for (const auto& alt_sol : res2.solutions) {
        if (alt_sol.grid != sol1) {
            found_different_answer = true;
            // Validate the alternative solution is legal for the unconstrained board
            auto val_alt = solvedin::validate_tango_solution(*b2, alt_sol.grid);
            CHECK_ASSERT(val_alt.is_valid);
            // Verify that it would violate the original edge clue
            auto val_with_original_clue = solvedin::validate_tango_solution(*b1, alt_sol.grid);
            CHECK_ASSERT(!val_with_original_clue.is_valid);
        }
    }
    CHECK_ASSERT(found_different_answer);

    std::cout << "[PASS] Tango edge clue dependency verified: ignoring clue produces different answer\n";
}

void test_tango_contradictory_and_malformed() {
    // Contradictory edge clue
    auto res_contra = solvedin::TangoBoard::load_from_file("fixtures/tango_contradictory.txt");
    CHECK_ASSERT(!res_contra.success);
    CHECK_ASSERT(res_contra.error_message.find("Contradictory edge clue") != std::string::npos);
    std::cout << "[PASS] Tango contradictory clue rejected: " << res_contra.error_message << "\n";

    // Malformed grid
    auto res_malformed = solvedin::TangoBoard::load_from_file("fixtures/tango_malformed.txt");
    CHECK_ASSERT(!res_malformed.success);
    CHECK_ASSERT(res_malformed.error_message.find("Malformed tango grid") != std::string::npos);
    std::cout << "[PASS] Tango malformed grid rejected: " << res_malformed.error_message << "\n";
}

void test_tango_validator_failures() {
    auto [success, maybe_board, err] = solvedin::TangoBoard::load_from_file("fixtures/tango_valid_unique.txt");
    CHECK_ASSERT(success);
    const auto& board = *maybe_board;

    auto result = solvedin::solve_tango(board, false);
    CHECK_ASSERT(result.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    auto grid = result.solutions[0].grid;

    // 1. Alter given symbol
    auto corrupted_given = grid;
    corrupted_given[0][0] = 'M'; // was given 'S'
    auto val1 = solvedin::validate_tango_solution(board, corrupted_given);
    CHECK_ASSERT(!val1.is_valid);
    CHECK_ASSERT(val1.error_message.find("Given symbol") != std::string::npos);

    // 2. Three consecutive identical symbols
    auto corrupted_three = grid;
    corrupted_three[1][0] = 'S';
    corrupted_three[1][1] = 'S';
    corrupted_three[1][2] = 'S';
    auto val2 = solvedin::validate_tango_solution(board, corrupted_three);
    CHECK_ASSERT(!val2.is_valid);

    // 3. Edge clue violation
    auto corrupted_clue = grid;
    // Violate horizontal clue (3, 1) '=': make grid[3][1] != grid[3][2]
    corrupted_clue[3][1] = (corrupted_clue[3][1] == 'S') ? 'M' : 'S';
    auto val3 = solvedin::validate_tango_solution(board, corrupted_clue);
    CHECK_ASSERT(!val3.is_valid);
    CHECK_ASSERT(val3.error_message.find("Horizontal clue '=' violated") != std::string::npos);

    std::cout << "[PASS] Tango validator failure detections\n";
}

int main() {
    std::cout << "Running Tango Tests...\n";
    test_tango_load_and_solve_unique();
    test_tango_edge_clue_dependency();
    test_tango_contradictory_and_malformed();
    test_tango_validator_failures();
    std::cout << "All Tango Tests Passed!\n";
    return 0;
}
