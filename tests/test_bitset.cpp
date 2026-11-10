#include "solvedin/bitset.hpp"
#include "solvedin/board.hpp"
#include "solvedin/queens_solver.hpp"
#include "solvedin/queens_validator.hpp"

#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

void test_bitset_boundary_size(size_t S) {
    using solvedin::BitSet;

    // 1. Zero-initialized bitset
    BitSet b(S, false);
    CHECK_ASSERT(b.size() == S);
    CHECK_ASSERT(b.count() == 0);
    CHECK_ASSERT(b.none());
    CHECK_ASSERT(!b.any());
    CHECK_ASSERT(b.find_first() == -1);

    // 2. All-ones bitset
    BitSet all_ones(S, true);
    CHECK_ASSERT(all_ones.size() == S);
    CHECK_ASSERT(all_ones.count() == S);
    CHECK_ASSERT(all_ones.any());
    CHECK_ASSERT(!all_ones.none());
    CHECK_ASSERT(all_ones.find_first() == 0);

    // 3. Inversion of all-ones must be all-zeros (no phantom bits in padded word)
    BitSet inverted_all = ~all_ones;
    CHECK_ASSERT(inverted_all.count() == 0);
    CHECK_ASSERT(inverted_all.none());

    // 4. Inversion of all-zeros must have exactly S set bits
    BitSet inverted_zero = ~b;
    CHECK_ASSERT(inverted_zero.count() == S);

    // 5. Boundary bits test
    // Test first bit, last bit, and word-boundary bits (63, 64, 127, 128) if in range
    std::set<size_t> boundary_bits = {0, S - 1};
    if (63 < S) boundary_bits.insert(63);
    if (64 < S) boundary_bits.insert(64);
    if (127 < S) boundary_bits.insert(127);
    if (128 < S) boundary_bits.insert(128);

    BitSet b_bits(S, false);
    for (size_t bit : boundary_bits) {
        b_bits.set(bit);
        CHECK_ASSERT(b_bits.test(bit));
    }

    CHECK_ASSERT(b_bits.count() == boundary_bits.size());

    // Invert b_bits
    BitSet b_inv = ~b_bits;
    CHECK_ASSERT(b_inv.count() == S - boundary_bits.size());
    for (size_t bit : boundary_bits) {
        CHECK_ASSERT(!b_inv.test(bit));
    }

    // Check AND / OR invariants
    BitSet b_and = b_bits & b_inv;
    CHECK_ASSERT(b_and.count() == 0);

    BitSet b_or = b_bits | b_inv;
    CHECK_ASSERT(b_or.count() == S);

    // 6. Test find_first and find_next across word boundaries
    if (S > 65) {
        BitSet cross(S, false);
        cross.set(63);
        cross.set(64);
        CHECK_ASSERT(cross.find_first() == 63);
        CHECK_ASSERT(cross.find_next(63) == 64);
        CHECK_ASSERT(cross.find_next(64) == -1);
    }

    if (S > 128) {
        BitSet cross2(S, false);
        cross2.set(127);
        cross2.set(128);
        CHECK_ASSERT(cross2.find_first() == 127);
        CHECK_ASSERT(cross2.find_next(127) == 128);
        CHECK_ASSERT(cross2.find_next(128) == -1);
    }

    // 7. Test for_each_set_bit
    std::vector<int> collected;
    b_bits.for_each_set_bit([&](int bit) {
        collected.push_back(bit);
    });
    CHECK_ASSERT(collected.size() == boundary_bits.size());
    for (size_t i = 0; i < collected.size(); ++i) {
        CHECK_ASSERT(b_bits.test(collected[i]));
    }

    std::cout << "[PASS] BitSet boundary test for size " << S << " (" << b.num_words() << " word(s))\n";
}

void test_all_bitset_boundaries() {
    // Specifically required sizes: 63, 64, 65, 127, 128, 129
    test_bitset_boundary_size(63);
    test_bitset_boundary_size(64);
    test_bitset_boundary_size(65);
    test_bitset_boundary_size(127);
    test_bitset_boundary_size(128);
    test_bitset_boundary_size(129);
}

void test_solver_bitset_multiword_no_exhaustive_search() {
    // Test that the Bitset solver initializes and performs placement constraint updates
    // for multi-word board sizes (such as N=9 -> 81 cells > 64, N=12 -> 144 cells > 128)
    // without requiring exhaustive searches at those sizes.

    // 1. Create a 9x9 board (81 cells -> spans 2 machine words)
    const int n9 = 9;
    std::vector<int> regions9(n9 * n9);
    for (int r = 0; r < n9; ++r) {
        for (int c = 0; c < n9; ++c) {
            regions9[r * n9 + c] = r; // Row-banded regions
        }
    }
    solvedin::Board board9(n9, regions9);

    // Solve for only 1 solution using bitset solver
    auto res9_bitset = solvedin::solve_queens(board9, false, solvedin::SolverVariant::BITSET);
    CHECK_ASSERT(res9_bitset.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(res9_bitset.solutions.size() == 1);
    auto val9 = solvedin::validate_queens_solution(board9, res9_bitset.solutions[0].queens);
    CHECK_ASSERT(val9.is_valid);
    std::cout << "[PASS] 9x9 board (81 cells, 2 words) solved with Bitset solver in "
              << res9_bitset.stats.nodes_visited << " nodes\n";

    // 2. Create a 12x12 board (144 cells -> spans 3 machine words)
    const int n12 = 12;
    std::vector<int> regions12(n12 * n12);
    for (int r = 0; r < n12; ++r) {
        for (int c = 0; c < n12; ++c) {
            regions12[r * n12 + c] = r;
        }
    }
    solvedin::Board board12(n12, regions12);

    auto res12_bitset = solvedin::solve_queens(board12, false, solvedin::SolverVariant::BITSET);
    CHECK_ASSERT(res12_bitset.status == solvedin::SolveStatus::UNIQUE_SOLUTION);
    CHECK_ASSERT(res12_bitset.solutions.size() == 1);
    auto val12 = solvedin::validate_queens_solution(board12, res12_bitset.solutions[0].queens);
    CHECK_ASSERT(val12.is_valid);
    std::cout << "[PASS] 12x12 board (144 cells, 3 words) solved with Bitset solver in "
              << res12_bitset.stats.nodes_visited << " nodes\n";
}

int main() {
    std::cout << "Running BitSet Boundary Tests...\n";
    test_all_bitset_boundaries();
    test_solver_bitset_multiword_no_exhaustive_search();
    std::cout << "All BitSet Boundary Tests Passed!\n";
    return 0;
}
