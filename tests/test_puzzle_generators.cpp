#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_generator.hpp"
#include "solvedin/sudoku_solver.hpp"
#include "solvedin/sudoku_validator.hpp"

#include "solvedin/tango_board.hpp"
#include "solvedin/tango_generator.hpp"
#include "solvedin/tango_solver.hpp"
#include "solvedin/tango_validator.hpp"

#include "solvedin/zip_board.hpp"
#include "solvedin/zip_generator.hpp"
#include "solvedin/zip_solver.hpp"
#include "solvedin/zip_validator.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;

// ============================================================================
// Mini Sudoku Generator Tests
// ============================================================================

void test_sudoku_generator_basic_and_uniqueness() {
    for (uint64_t seed : {42ULL, 12345ULL, 987654321ULL}) {
        auto board = generate_sudoku(seed, 20);

        // Check givens count: should have at least 1 given and <= 36
        int givens = board.givens_count();
        CHECK_ASSERT(givens >= 1 && givens <= 36);

        // Verify format and parsing
        std::string formatted = format_sudoku(board);
        auto parse_res = SudokuBoard::parse_from_string(formatted);
        CHECK_ASSERT(parse_res.success);
        CHECK_ASSERT(parse_res.board.has_value());
        CHECK_ASSERT(parse_res.board->givens_count() == givens);

        // Verify that solving with check_unique=true yields UNIQUE_SOLUTION
        auto solve_res = solve_sudoku(board, true);
        CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
        CHECK_ASSERT(solve_res.solutions.size() == 1);

        // Verify solution independently with validator
        auto val_res = validate_sudoku_solution(board, solve_res.solutions[0].grid);
        CHECK_ASSERT(val_res.is_valid);
        CHECK_ASSERT(val_res.error_message.empty());

        std::cout << "[PASS] Mini Sudoku generator seed " << seed << " (givens=" << givens << ")\n";
    }
}

void test_sudoku_generator_determinism() {
    auto b1 = generate_sudoku(777ULL, 18);
    auto b2 = generate_sudoku(777ULL, 18);
    auto b3 = generate_sudoku(888ULL, 18);

    CHECK_ASSERT(b1.grid() == b2.grid());
    CHECK_ASSERT(b1.grid() != b3.grid());
    std::cout << "[PASS] Mini Sudoku generator determinism\n";
}

void test_sudoku_generator_various_removals() {
    for (int removals : {10, 16, 20, 24}) {
        auto board = generate_sudoku(1337ULL, removals);
        auto solve_res = solve_sudoku(board, true);
        CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
        CHECK_ASSERT(solve_res.solutions.size() == 1);

        auto val_res = validate_sudoku_solution(board, solve_res.solutions[0].grid);
        CHECK_ASSERT(val_res.is_valid);
    }
    std::cout << "[PASS] Mini Sudoku generator various removal counts\n";
}

// ============================================================================
// Tango Generator Tests
// ============================================================================

void test_tango_generator_basic_and_uniqueness() {
    for (uint64_t seed : {101ULL, 2024ULL, 55555ULL}) {
        auto board = generate_tango(seed, 8, 6);
        CHECK_ASSERT(board.size() == 6);

        // Verify format and roundtrip parsing
        std::string formatted = board.to_string();
        auto parse_res = TangoBoard::parse_from_string(formatted);
        CHECK_ASSERT(parse_res.success);
        CHECK_ASSERT(parse_res.board.has_value());
        CHECK_ASSERT(parse_res.board->size() == 6);

        // Verify that solving with check_unique=true yields UNIQUE_SOLUTION
        auto solve_res = solve_tango(board, true);
        CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
        CHECK_ASSERT(solve_res.solutions.size() == 1);

        // Verify solution independently with validator
        auto val_res = validate_tango_solution(board, solve_res.solutions[0].grid);
        CHECK_ASSERT(val_res.is_valid);
        CHECK_ASSERT(val_res.error_message.empty());

        std::cout << "[PASS] Tango generator seed " << seed << "\n";
    }
}

void test_tango_generator_determinism() {
    auto b1 = generate_tango(999ULL, 8, 6);
    auto b2 = generate_tango(999ULL, 8, 6);
    auto b3 = generate_tango(111ULL, 8, 6);

    CHECK_ASSERT(b1.grid() == b2.grid());
    CHECK_ASSERT(b1.horizontal_clues() == b2.horizontal_clues());
    CHECK_ASSERT(b1.vertical_clues() == b2.vertical_clues());

    CHECK_ASSERT(b1.grid() != b3.grid() || b1.horizontal_clues() != b3.horizontal_clues());
    std::cout << "[PASS] Tango generator determinism\n";
}

void test_tango_generator_clue_combinations() {
    // Test combinations of givens and edge clues
    std::vector<std::pair<int, int>> configs = {
        {10, 4},
        {8, 6},
        {6, 8},
        {12, 3}
    };

    for (const auto& [givens, edges] : configs) {
        auto board = generate_tango(31415ULL, givens, edges);
        auto solve_res = solve_tango(board, true);
        CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
        CHECK_ASSERT(solve_res.solutions.size() == 1);

        auto val_res = validate_tango_solution(board, solve_res.solutions[0].grid);
        CHECK_ASSERT(val_res.is_valid);
    }
    std::cout << "[PASS] Tango generator clue combinations\n";
}

// ============================================================================
// Zip Generator Tests
// ============================================================================

void test_zip_generator_basic_and_uniqueness() {
    std::vector<std::tuple<int, int, int, uint64_t>> test_cases = {
        {4, 4, 4, 1001ULL},
        {4, 4, 5, 2002ULL},
        {5, 5, 5, 3003ULL},
        {6, 6, 6, 4004ULL},
        {3, 3, 3, 5005ULL},
        {3, 5, 4, 6006ULL}
    };

    for (const auto& [r, c, cps, seed] : test_cases) {
        auto board = generate_zip(r, c, cps, seed);
        CHECK_ASSERT(board.rows() == r);
        CHECK_ASSERT(board.cols() == c);
        CHECK_ASSERT(board.max_checkpoint() == cps);

        // Verify format and roundtrip parsing
        std::string formatted = board.to_string();
        auto parse_res = ZipBoard::parse_from_string(formatted);
        CHECK_ASSERT(parse_res.success);
        CHECK_ASSERT(parse_res.board.has_value());
        CHECK_ASSERT(parse_res.board->rows() == r);
        CHECK_ASSERT(parse_res.board->cols() == c);
        CHECK_ASSERT(parse_res.board->max_checkpoint() == cps);

        // Verify that solving with check_unique=true yields UNIQUE_SOLUTION
        auto solve_res = solve_zip(board, true);
        CHECK_ASSERT(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
        CHECK_ASSERT(solve_res.solutions.size() == 1);

        // Verify solution independently with validator
        auto val_res = validate_zip_solution(board, solve_res.solutions[0].path);
        CHECK_ASSERT(val_res.is_valid);
        CHECK_ASSERT(val_res.error_message.empty());

        std::cout << "[PASS] Zip generator " << r << "x" << c << " cps=" << cps << " seed=" << seed << "\n";
    }
}

void test_zip_generator_determinism() {
    auto b1 = generate_zip(4, 4, 5, 12345ULL);
    auto b2 = generate_zip(4, 4, 5, 12345ULL);
    auto b3 = generate_zip(4, 4, 5, 54321ULL);

    CHECK_ASSERT(b1.grid() == b2.grid());
    CHECK_ASSERT(b1.h_walls() == b2.h_walls());
    CHECK_ASSERT(b1.v_walls() == b2.v_walls());

    CHECK_ASSERT(b1.grid() != b3.grid() || b1.h_walls() != b3.h_walls() || b1.v_walls() != b3.v_walls());
    std::cout << "[PASS] Zip generator determinism\n";
}

void test_zip_generator_edge_cases() {
    // Invalid size should throw invalid_argument
    bool threw = false;
    try {
        generate_zip(1, 4, 2, 42ULL);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK_ASSERT(threw);

    // Checkpoint count clamped to total cells
    auto board = generate_zip(3, 3, 50, 42ULL);
    CHECK_ASSERT(board.max_checkpoint() == 9);
    auto res = solve_zip(board, true);
    CHECK_ASSERT(res.status == SolveStatus::UNIQUE_SOLUTION);

    std::cout << "[PASS] Zip generator edge cases\n";
}

int main() {
    std::cout << "=============================================\n";
    std::cout << "Running Puzzle Generators Unit Tests\n";
    std::cout << "=============================================\n\n";

    std::cout << "--- Mini Sudoku Generator ---\n";
    test_sudoku_generator_basic_and_uniqueness();
    test_sudoku_generator_determinism();
    test_sudoku_generator_various_removals();
    std::cout << "\n";

    std::cout << "--- Tango Generator ---\n";
    test_tango_generator_basic_and_uniqueness();
    test_tango_generator_determinism();
    test_tango_generator_clue_combinations();
    std::cout << "\n";

    std::cout << "--- Zip Generator ---\n";
    test_zip_generator_basic_and_uniqueness();
    test_zip_generator_determinism();
    test_zip_generator_edge_cases();
    std::cout << "\n";

    std::cout << "=============================================\n";
    std::cout << "All Puzzle Generator Tests PASSED Successfully!\n";
    std::cout << "=============================================\n";
    return 0;
}
