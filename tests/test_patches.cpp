#include "solvedin/patches_board.hpp"
#include "solvedin/patches_solver.hpp"
#include "solvedin/patches_validator.hpp"

#include <cassert>
#include <iostream>

using namespace solvedin;

void test_patches_parser() {
    std::cout << "Testing PatchesBoard parser..." << std::endl;

    auto res = PatchesBoard::load_from_file("fixtures/patches_valid_unique.txt");
    assert(res.success);
    assert(res.board.has_value());
    const auto& b = *res.board;
    (void)b;
    assert(b.rows() == 6);
    assert(b.cols() == 6);
    assert(b.num_cells() == 36);
    assert(b.num_clues() == 9);

    // Verify clues:
    // (0,0): 4S (both)
    auto c00 = b.clue_at(0, 0);
    (void)c00;
    assert(c00.has_value());
    assert(c00->size == 4);
    assert(c00->shape == ShapeConstraint::SQUARE);

    // (0,2): 6W (both)
    auto c02 = b.clue_at(0, 2);
    (void)c02;
    assert(c02.has_value());
    assert(c02->size == 6);
    assert(c02->shape == ShapeConstraint::WIDE);

    // (0,5): 4T (both)
    auto c05 = b.clue_at(0, 5);
    (void)c05;
    assert(c05.has_value());
    assert(c05->size == 4);
    assert(c05->shape == ShapeConstraint::TALL);

    // (2,0): 4F (freeform + size)
    auto c20 = b.clue_at(2, 0);
    (void)c20;
    assert(c20.has_value());
    assert(c20->size == 4);
    assert(c20->shape == ShapeConstraint::FREEFORM);

    // (2,3): 4 (size only)
    auto c23 = b.clue_at(2, 3);
    (void)c23;
    assert(c23.has_value());
    assert(c23->size == 4);
    assert(!c23->has_shape());

    // (3,2): ? (neither)
    auto c32 = b.clue_at(3, 2);
    (void)c32;
    assert(c32.has_value());
    assert(!c32->has_size());
    assert(!c32->has_shape());

    // (4,0): 4S (both)
    auto c40 = b.clue_at(4, 0);
    (void)c40;
    assert(c40.has_value());
    assert(c40->size == 4);
    assert(c40->shape == ShapeConstraint::SQUARE);

    // (4,2): T (shape only)
    auto c42 = b.clue_at(4, 2);
    (void)c42;
    assert(c42.has_value());
    assert(!c42->has_size());
    assert(c42->shape == ShapeConstraint::TALL);

    // (4,3): 6W (both)
    auto c43 = b.clue_at(4, 3);
    (void)c43;
    assert(c43.has_value());
    assert(c43->size == 6);
    assert(c43->shape == ShapeConstraint::WIDE);

    // Test malformed fixture
    auto mal = PatchesBoard::load_from_file("fixtures/patches_malformed.txt");
    assert(!mal.success);

    // Test empty input
    auto emp = PatchesBoard::parse_from_string("");
    assert(!emp.success);

    // Test no clues
    auto noc = PatchesBoard::parse_from_string("2 2\nGRID\n. .\n. .\n");
    assert(!noc.success);

    std::cout << "  PatchesBoard parser passed." << std::endl;
}

void test_patches_validator() {
    std::cout << "Testing Patches validator..." << std::endl;

    auto res = PatchesBoard::load_from_file("fixtures/patches_valid_unique.txt");
    assert(res.success);
    const auto& b = *res.board;
    (void)b;

    // Manually construct the expected solution (all solid rectangles):
    // P1: (0,0), (0,1), (1,0), (1,1) -> 4S at (0,0)
    // P2: (0,2..4), (1,2..4) -> 6W at (0,2)
    // P3: (0..3, 5) -> 4T at (0,5)
    // P4: (2..3, 0..1) -> 4F at (2,0) [2x2 rectangle]
    // P5: (2..3, 3..4) -> 4 at (2,3) [2x2 rectangle]
    // P6: (2..3, 2) -> ? at (3,2) [2x1 rectangle]
    // P7: (4..5, 0..1) -> 4S at (4,0) [2x2 rectangle]
    // P8: (4..5, 2) -> T at (4,2) [2x1 rectangle]
    // P9: (4..5, 3..5) -> 6W at (4,3) [2x3 rectangle]
    PatchesSolution sol;
    sol.patches = {
        { {0, 0}, { {0, 0}, {0, 1}, {1, 0}, {1, 1} } },
        { {0, 2}, { {0, 2}, {0, 3}, {0, 4}, {1, 2}, {1, 3}, {1, 4} } },
        { {0, 5}, { {0, 5}, {1, 5}, {2, 5}, {3, 5} } },
        { {2, 0}, { {2, 0}, {2, 1}, {3, 0}, {3, 1} } },
        { {2, 3}, { {2, 3}, {2, 4}, {3, 3}, {3, 4} } },
        { {3, 2}, { {2, 2}, {3, 2} } },
        { {4, 0}, { {4, 0}, {4, 1}, {5, 0}, {5, 1} } },
        { {4, 2}, { {4, 2}, {5, 2} } },
        { {4, 3}, { {4, 3}, {4, 4}, {4, 5}, {5, 3}, {5, 4}, {5, 5} } }
    };

    auto val = validate_patches_solution(b, sol);
    assert(val.is_valid);

    // Error: missing a cell (incomplete coverage)
    auto bad_cov = sol;
    bad_cov.patches[0].cells.pop_back();
    assert(!validate_patches_solution(b, bad_cov).is_valid);

    // Error: overlapping cells
    auto bad_overlap = sol;
    bad_overlap.patches[1].cells.push_back(Position{0, 0});
    assert(!validate_patches_solution(b, bad_overlap).is_valid);

    // Error: non-rectangular patch (L-tromino) rejected
    auto bad_rect = sol;
    bad_rect.patches[0].cells = { {0, 0}, {0, 1}, {1, 0} };
    assert(!validate_patches_solution(b, bad_rect).is_valid);

    // Error: size mismatch for clue with size
    auto bad_size = sol;
    bad_size.patches[0].cells = { {0, 0}, {0, 1}, {1, 0} };
    assert(!validate_patches_solution(b, bad_size).is_valid);

    // Error: shape mismatch (square clue assigned non-square rectangle)
    auto single_board_res = PatchesBoard::parse_from_string("1 2\nGRID\nS .\n");
    assert(single_board_res.success);
    PatchesSolution bad_shape_sol;
    bad_shape_sol.patches = { { {0, 0}, { {0, 0}, {0, 1} } } }; // 1x2 wide rectangle for S clue!
    assert(!validate_patches_solution(*single_board_res.board, bad_shape_sol).is_valid);

    std::cout << "  Patches validator passed." << std::endl;
}

void test_patches_solver() {
    std::cout << "Testing Patches solver..." << std::endl;

    // Test unique solve on 6x6
    auto res = PatchesBoard::load_from_file("fixtures/patches_valid_unique.txt");
    assert(res.success);
    const auto& b = *res.board;
    (void)b;

    auto solve_res = solve_patches(b, true);
    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res.solutions.size() == 1);
    assert(solve_res.solutions[0].patches.size() == 9);

    // Validate returned solution
    auto val = validate_patches_solution(b, solve_res.solutions[0]);
    assert(val.is_valid);

    // Test freeform fixture with legal rectangular patches
    auto res_ff = PatchesBoard::load_from_file("fixtures/patches_freeform.txt");
    assert(res_ff.success);
    auto solve_ff = solve_patches(*res_ff.board, true);
    assert(solve_ff.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_ff.solutions.size() == 1);
    auto val_ff = validate_patches_solution(*res_ff.board, solve_ff.solutions[0]);
    assert(val_ff.is_valid);

    // Regression test on 2x2 grid with 3F and 1S:
    // This board can only be covered by an L-shaped patch, so solver must report NO_SOLUTION
    // and validator must reject any non-rectangular cover.
    auto res_reg = PatchesBoard::load_from_file("fixtures/patches_regression_2x2.txt");
    assert(res_reg.success);
    auto solve_reg = solve_patches(*res_reg.board, true);
    assert(solve_reg.status == SolveStatus::NO_SOLUTION);
    assert(solve_reg.solutions.empty());

    // Validator rejects the L-shaped cover on 2x2
    PatchesSolution l_tromino_sol;
    l_tromino_sol.patches = {
        { {0, 0}, { {0, 0}, {0, 1}, {1, 0} } }, // L-tromino (non-rectangular!)
        { {1, 1}, { {1, 1} } }
    };
    auto val_reg = validate_patches_solution(*res_reg.board, l_tromino_sol);
    assert(!val_reg.is_valid);

    // Test contradictory fixture
    auto res_contra = PatchesBoard::load_from_file("fixtures/patches_contradictory.txt");
    assert(res_contra.success);
    auto solve_contra = solve_patches(*res_contra.board, true);
    assert(solve_contra.status == SolveStatus::NO_SOLUTION);

    // Test budget exhaustion
    SolveOptions tiny_budget{.max_nodes = 1};
    auto solve_budget = solve_patches(b, false, tiny_budget);
    assert(solve_budget.status == SolveStatus::INCONCLUSIVE);
    assert(solve_budget.stats.budget_exhausted);

    std::cout << "  Patches solver passed." << std::endl;
}

int main() {
    test_patches_parser();
    test_patches_validator();
    test_patches_solver();
    std::cout << "All Patches tests passed successfully!" << std::endl;
    return 0;
}
