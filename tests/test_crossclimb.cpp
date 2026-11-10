#include "solvedin/crossclimb_board.hpp"
#include "solvedin/crossclimb_solver.hpp"
#include "solvedin/crossclimb_validator.hpp"
#include "solvedin/trie.hpp"

#include <cassert>
#include <iostream>

using namespace solvedin;

void test_crossclimb_parser() {
    std::cout << "Testing Crossclimb parser..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_valid_unique.txt");
    assert(res.success);
    assert(res.board.has_value());
    [[maybe_unused]] const auto& b = *res.board;
    assert(b.word_length() == 4);
    assert(b.has_start_word());
    assert(*b.start_word() == "COLD");
    assert(b.num_rungs() == 3);
    assert(b.has_end_word());
    assert(*b.end_word() == "WARN");

    // Malformed fixture
    auto malformed = CrossclimbBoard::load_from_file("fixtures/crossclimb_malformed.txt");
    assert(!malformed.success);
    assert(!malformed.error_message.empty());

    // Missing rungs
    auto empty_rungs = CrossclimbBoard::parse_from_string("WORD_LENGTH: 4\nSTART: COLD\nEND: WARM\n");
    assert(!empty_rungs.success);
}

void test_crossclimb_solve_unique() {
    std::cout << "Testing Crossclimb solve unique..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_valid_unique.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res.solutions.size() == 1);

    const auto& sol = solve_res.solutions[0];
    auto val_res = CrossclimbValidator::validate(board, sol, lexicon);
    assert(val_res.is_valid);
    assert(val_res.error_message.empty());

    // Expected ladder: COLD -> CORD -> CARD -> WARD -> WARN
    assert(sol.ladder.size() == 5);
    assert(sol.ladder[0] == "COLD");
    assert(sol.ladder[1] == "CORD");
    assert(sol.ladder[2] == "CARD");
    assert(sol.ladder[3] == "WARD");
    assert(sol.ladder[4] == "WARN");
}

void test_crossclimb_repeated_letters() {
    std::cout << "Testing Crossclimb repeated letters..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_repeated_letters.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION || solve_res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(!solve_res.solutions.empty());

    for (const auto& sol : solve_res.solutions) {
        auto val_res = CrossclimbValidator::validate(board, sol, lexicon);
        assert(val_res.is_valid);
    }
}

void test_crossclimb_ambiguous() {
    std::cout << "Testing Crossclimb ambiguous (multiple valid ladders)..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_ambiguous.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(solve_res.solutions.size() >= 2);

    for (const auto& sol : solve_res.solutions) {
        auto val_res = CrossclimbValidator::validate(board, sol, lexicon);
        assert(val_res.is_valid);
    }
}

void test_crossclimb_missing_endpoints() {
    std::cout << "Testing Crossclimb missing endpoints with clue candidates..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_missing_endpoints.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION || solve_res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(!solve_res.solutions.empty());

    for (const auto& sol : solve_res.solutions) {
        auto val_res = CrossclimbValidator::validate(board, sol, lexicon);
        assert(val_res.is_valid);
    }
}

void test_crossclimb_no_solution() {
    std::cout << "Testing Crossclimb no solution..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_no_solution.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    CrossclimbSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::NO_SOLUTION);
    assert(solve_res.solutions.empty());
}

void test_crossclimb_validator_negative() {
    std::cout << "Testing Crossclimb independent validator negative cases..." << std::endl;

    auto res = CrossclimbBoard::load_from_file("fixtures/crossclimb_valid_unique.txt");
    assert(res.success);
    const auto& board = *res.board;
    Trie lexicon = Trie::get_default_lexicon();

    // Word not in dictionary
    CrossclimbSolution bad_lexicon{{"COLD", "CORD", "XXXX", "WARD", "WARN"}};
    auto v1 = CrossclimbValidator::validate(board, bad_lexicon, lexicon);
    assert(!v1.is_valid);

    // Hamming distance > 1
    CrossclimbSolution bad_dist{{"COLD", "CARD", "CORD", "WARD", "WARN"}}; // COLD to CARD dist=2
    auto v2 = CrossclimbValidator::validate(board, bad_dist, lexicon);
    assert(!v2.is_valid);

    // Duplicate word
    CrossclimbSolution duplicate{{"COLD", "CORD", "CORD", "WARD", "WARN"}};
    auto v3 = CrossclimbValidator::validate(board, duplicate, lexicon);
    assert(!v3.is_valid);

    // Missing required rung
    CrossclimbSolution missing_rung{{"COLD", "CORD", "BOLD", "HOLD", "WARN"}};
    auto v4 = CrossclimbValidator::validate(board, missing_rung, lexicon);
    assert(!v4.is_valid);
}

int main() {
    test_crossclimb_parser();
    test_crossclimb_solve_unique();
    test_crossclimb_repeated_letters();
    test_crossclimb_ambiguous();
    test_crossclimb_missing_endpoints();
    test_crossclimb_no_solution();
    test_crossclimb_validator_negative();

    std::cout << "All Crossclimb tests passed successfully!" << std::endl;
    return 0;
}
