#include "solvedin/zip_board.hpp"
#include "solvedin/zip_solver.hpp"
#include "solvedin/zip_validator.hpp"

#include <cassert>
#include <iostream>

using namespace solvedin;

void test_zip_parser() {
    std::cout << "Testing ZipBoard parser..." << std::endl;

    // Test loading valid unique fixture
    auto res = ZipBoard::load_from_file("fixtures/zip_valid_unique.txt");
    assert(res.success);
    assert(res.board.has_value());
    const auto& b = *res.board;
    (void)b;
    assert(b.rows() == 4);
    assert(b.cols() == 4);
    assert(b.num_cells() == 16);
    assert(b.max_checkpoint() == 5);
    assert(b.checkpoint_at(0, 0) == 1);
    assert(b.checkpoint_at(0, 3) == 2);
    assert(b.checkpoint_at(2, 0) == 3);
    assert(b.checkpoint_at(3, 3) == 4);
    assert(b.checkpoint_at(3, 0) == 5);
    assert(b.checkpoint_pos(1) == Position(0, 0));
    assert(b.checkpoint_pos(2) == Position(0, 3));
    assert(b.checkpoint_pos(3) == Position(2, 0));
    assert(b.checkpoint_pos(4) == Position(3, 3));
    assert(b.checkpoint_pos(5) == Position(3, 0));

    // Walls in 4x4
    assert(b.has_h_wall(0, 0));
    assert(b.has_h_wall(0, 1));
    assert(b.has_h_wall(0, 2));
    assert(!b.has_h_wall(0, 3));
    assert(!b.can_move(Position(0, 0), Position(1, 0)));
    assert(b.can_move(Position(0, 3), Position(1, 3)));

    // Test malformed: missing checkpoint 2
    auto malformed_res = ZipBoard::load_from_file("fixtures/zip_malformed.txt");
    assert(!malformed_res.success);
    assert(!malformed_res.board.has_value());

    // Test empty input
    auto empty_res = ZipBoard::parse_from_string("");
    assert(!empty_res.success);

    // Test single checkpoint (requires at least 1 and 2)
    auto single_cp = ZipBoard::parse_from_string("2 2\nGRID\n1 .\n. .\n");
    assert(!single_cp.success);

    // Test duplicate checkpoint
    auto dup_cp = ZipBoard::parse_from_string("2 2\nGRID\n1 2\n2 .\n");
    assert(!dup_cp.success);

    std::cout << "  ZipBoard parser passed." << std::endl;
}

void test_zip_validator() {
    std::cout << "Testing Zip validator..." << std::endl;

    auto res = ZipBoard::load_from_file("fixtures/zip_valid_unique.txt");
    assert(res.success);
    const auto& b = *res.board;
    (void)b;

    // Expected valid path for 4x4 fixture:
    // (0,0)=1 -> (0,1) -> (0,2) -> (0,3)=2 -> (1,3) -> (1,2) -> (1,1) -> (1,0)
    // -> (2,0)=3 -> (2,1) -> (2,2) -> (2,3) -> (3,3)=4 -> (3,2) -> (3,1) -> (3,0)=5
    std::vector<Position> valid_path = {
        {0, 0}, {0, 1}, {0, 2}, {0, 3},
        {1, 3}, {1, 2}, {1, 1}, {1, 0},
        {2, 0}, {2, 1}, {2, 2}, {2, 3},
        {3, 3}, {3, 2}, {3, 1}, {3, 0}
    };

    auto val = validate_zip_solution(b, valid_path);
    assert(val.is_valid);
    assert(val.error_message.empty());

    // Error: wrong start checkpoint
    auto bad_start = valid_path;
    bad_start[0] = Position{0, 1};
    assert(!validate_zip_solution(b, bad_start).is_valid);

    // Error: wrong end checkpoint
    auto bad_end = valid_path;
    bad_end.back() = Position{3, 1};
    assert(!validate_zip_solution(b, bad_end).is_valid);

    // Error: path too short
    auto short_path = valid_path;
    short_path.pop_back();
    assert(!validate_zip_solution(b, short_path).is_valid);

    // Error: duplicate cell
    auto dup_cell_path = valid_path;
    dup_cell_path[1] = dup_cell_path[0];
    assert(!validate_zip_solution(b, dup_cell_path).is_valid);

    // Error: crossing wall (try stepping down from (0,0) to (1,0) where h_wall exists)
    std::vector<Position> wall_cross_path = valid_path;
    wall_cross_path[1] = Position{1, 0}; // crosses h_wall(0,0)
    assert(!validate_zip_solution(b, wall_cross_path).is_valid);

    // Error: checkpoint out of order
    // In a 3x3 board where checkpoints are 1, 2, 3, 4
    auto cp_order_res = ZipBoard::load_from_file("fixtures/zip_checkpoint_order.txt");
    assert(cp_order_res.success);
    const auto& cp_board = *cp_order_res.board;
    (void)cp_board;

    // Correct path:
    // (0,0)[1] -> (1,0) -> (2,0)[2] -> (2,1) -> (1,1) -> (0,1) -> (0,2)[3] -> (1,2) -> (2,2)[4]
    std::vector<Position> correct_order_path = {
        {0, 0}, {1, 0}, {2, 0}, {2, 1}, {1, 1}, {0, 1}, {0, 2}, {1, 2}, {2, 2}
    };
    auto val_order = validate_zip_solution(cp_board, correct_order_path);
    assert(val_order.is_valid);

    // Now try order where 3 is visited before 2
    std::vector<Position> wrong_order_path = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 2}, {1, 2}, {2, 2}, {2, 1}, {2, 0}
    };
    auto val_wrong = validate_zip_solution(cp_board, wrong_order_path);
    assert(!val_wrong.is_valid);

    std::cout << "  Zip validator passed." << std::endl;
}

void test_zip_solver() {
    std::cout << "Testing Zip solver..." << std::endl;

    // Test unique solve on 4x4
    auto res = ZipBoard::load_from_file("fixtures/zip_valid_unique.txt");
    assert(res.success);
    const auto& b = *res.board;
    (void)b;

    auto solve_res = solve_zip(b, true);
    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res.solutions.size() == 1);
    assert(solve_res.solutions[0].path.size() == 16);

    // Validate returned path
    auto val = validate_zip_solution(b, solve_res.solutions[0].path);
    assert(val.is_valid);

    // Test checkpoint order fixture
    auto res2 = ZipBoard::load_from_file("fixtures/zip_checkpoint_order.txt");
    assert(res2.success);
    auto solve_res2 = solve_zip(*res2.board, true);
    assert(solve_res2.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res2.solutions.size() == 1);
    auto val2 = validate_zip_solution(*res2.board, solve_res2.solutions[0].path);
    assert(val2.is_valid);

    // Test blocked fixture: NO_SOLUTION
    auto res_blocked = ZipBoard::load_from_file("fixtures/zip_walls_blocked.txt");
    assert(res_blocked.success);
    auto solve_blocked = solve_zip(*res_blocked.board, true);
    assert(solve_blocked.status == SolveStatus::NO_SOLUTION);
    assert(solve_blocked.solutions.empty());

    // Test budget exhaustion: INCONCLUSIVE
    SolveOptions tiny_budget{.max_nodes = 2};
    auto solve_budget = solve_zip(b, false, tiny_budget);
    assert(solve_budget.status == SolveStatus::INCONCLUSIVE);
    assert(solve_budget.stats.budget_exhausted);

    std::cout << "  Zip solver passed." << std::endl;
}

int main() {
    test_zip_parser();
    test_zip_validator();
    test_zip_solver();
    std::cout << "All Zip tests passed successfully!" << std::endl;
    return 0;
}
