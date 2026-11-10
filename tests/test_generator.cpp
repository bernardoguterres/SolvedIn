#include "solvedin/board.hpp"
#include "solvedin/queens_generator.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"

#include <cstdlib>
#include <iostream>
#include <sstream>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_generator_sizes() {
    for (int n : {4, 6, 8, 12, 16, 32}) {
        auto gen = solvedin::QueensGenerator::generate(n, 12345ULL + n);
        const auto& board = gen.board;
        const auto& planted = gen.planted_queens;

        CHECK_ASSERT(board.size() == n);
        CHECK_ASSERT(static_cast<int>(planted.size()) == n);
        CHECK_ASSERT(static_cast<int>(board.distinct_regions().size()) == n);

        // Independent validation of planted solution
        auto val = solvedin::validate_queens_solution(board, planted);
        CHECK_ASSERT(val.is_valid);

        // Independent connectivity verification
        bool conn = solvedin::QueensGenerator::verify_region_connectivity(board, planted);
        CHECK_ASSERT(conn);

        std::cout << "[PASS] Generator verified for size N=" << n << "\n";
    }
}

void test_generator_determinism() {
    auto gen1 = solvedin::QueensGenerator::generate(8, 9999ULL);
    auto gen2 = solvedin::QueensGenerator::generate(8, 9999ULL);
    auto gen3 = solvedin::QueensGenerator::generate(8, 8888ULL);

    CHECK_ASSERT(gen1.board.regions() == gen2.board.regions());
    CHECK_ASSERT(gen1.planted_queens == gen2.planted_queens);

    // Different seed should produce different placement
    CHECK_ASSERT(gen1.planted_queens != gen3.planted_queens);

    std::cout << "[PASS] Generator determinism test\n";
}

void test_generator_roundtrip_solve() {
    auto gen = solvedin::QueensGenerator::generate(8, 42ULL);

    // Serialize to solver format
    std::ostringstream ss;
    ss << gen.board.size() << "\n";
    for (int r = 0; r < gen.board.size(); ++r) {
        for (int c = 0; c < gen.board.size(); ++c) {
            ss << gen.board.get_region(r, c) << " ";
        }
        ss << "\n";
    }

    // Parse back without knowing planted queen positions
    auto parsed = solvedin::Board::parse_from_string(ss.str());
    CHECK_ASSERT(parsed.success);
    CHECK_ASSERT(parsed.board.has_value());

    // Solve with Bitset solver
    auto res = solvedin::solve_queens(*parsed.board, false, solvedin::SolverVariant::BITSET);
    CHECK_ASSERT(res.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(res.solutions.size() == 1);

    auto val = solvedin::validate_queens_solution(*parsed.board, res.solutions[0].queens);
    CHECK_ASSERT(val.is_valid);

    std::cout << "[PASS] Generator roundtrip solve test\n";
}

int main() {
    std::cout << "Running Queens Generator Tests...\n";
    test_generator_sizes();
    test_generator_determinism();
    test_generator_roundtrip_solve();
    std::cout << "All Queens Generator Tests Passed!\n";
    return 0;
}
