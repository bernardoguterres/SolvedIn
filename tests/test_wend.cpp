#include <set>
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"
#include "solvedin/wend_solver.hpp"
#include "solvedin/wend_validator.hpp"

#include <cassert>
#include <iostream>
#include <string>

using namespace solvedin;

void test_wend_parser() {
    std::cout << "Testing Wend parser..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_valid_unique.txt");
    assert(res.success);
    assert(res.board.has_value());
    [[maybe_unused]] const auto& b = *res.board;
    assert(b.rows() == 3);
    assert(b.cols() == 3);
    assert(b.letter_at(0, 0) == 'F');
    assert(b.letter_at(1, 1) == 'A');
    assert(b.letter_at(2, 2) == 'G');
    assert(b.target_lengths().size() == 3);
    assert(b.target_lengths()[0] == 3);

    // Test malformed
    auto malformed = WendBoard::load_from_file("fixtures/wend_malformed.txt");
    assert(!malformed.success);
    assert(!malformed.error_message.empty());

    // Test missing dimensions
    auto bad_dims = WendBoard::parse_from_string("GRID\nC A T\n");
    assert(!bad_dims.success);

    // Test invalid characters
    auto bad_chars = WendBoard::parse_from_string("1 3\n1 2 3\nLENGTHS: 3\n");
    assert(!bad_chars.success);
}

void test_wend_distinct_path_retention() {
    std::cout << "Testing Wend distinct path retention for duplicate words..." << std::endl;

    // A 2x3 grid where "CAT" can be formed along two different paths
    // C A T
    // C A T
    std::string grid_str = "2 3\nC A T\nC A T\nLENGTHS: 3 3\n";
    auto parse_res = WendBoard::parse_from_string(grid_str);
    assert(parse_res.success);

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(*parse_res.board, lexicon);
    auto candidates = solver.enumerate_candidate_paths();

    int cat_count = 0;
    for (const auto& cand : candidates) {
        if (cand.word == "CAT") {
            cat_count++;
        }
    }
    // There are 2 disjoint paths spelling CAT: row 0 and row 1
    assert(cat_count >= 2);
    (void)cat_count;
}

void test_wend_solve_unique() {
    std::cout << "Testing Wend unique solve..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_valid_unique.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res.solutions.size() == 1);

    const auto& sol = solve_res.solutions[0];
    assert(sol.words.size() == 3);

    // Check independent validator
    auto val_res = WendValidator::validate(board, sol, lexicon);
    assert(val_res.is_valid);
}

void test_wend_repeated_letters() {
    std::cout << "Testing Wend with repeated letters in grid..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_repeated_letters.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve(false);

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION || solve_res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(!solve_res.solutions.empty());

    const auto& sol = solve_res.solutions[0];
    auto val_res = WendValidator::validate(board, sol, lexicon);
    assert(val_res.is_valid);
}

void test_wend_ambiguous() {
    std::cout << "Testing Wend ambiguous fixture..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_ambiguous.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve(true);

    assert(solve_res.status == SolveStatus::MULTIPLE_SOLUTIONS);
    assert(solve_res.solutions.size() >= 2);

    for (const auto& sol : solve_res.solutions) {
        auto val = WendValidator::validate(board, sol, lexicon);
        assert(val.is_valid);
    }
}

void test_wend_no_solution() {
    std::cout << "Testing Wend no solution..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_no_solution.txt");
    assert(res.success);
    const auto& board = *res.board;

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::NO_SOLUTION);
    assert(solve_res.solutions.empty());
}

void test_wend_validator_negative() {
    std::cout << "Testing Wend independent validator negative cases..." << std::endl;

    auto res = WendBoard::load_from_file("fixtures/wend_valid_unique.txt");
    assert(res.success);
    const auto& board = *res.board;
    Trie lexicon = Trie::get_default_lexicon();

    WendSolution bad_words;
    // Word not in dictionary
    bad_words.words.push_back(WendWordPath{
        "XYZ", {Position{0, 0}, Position{0, 1}, Position{0, 2}}
    });
    bad_words.words.push_back(WendWordPath{
        "VAN", {Position{1, 0}, Position{1, 1}, Position{1, 2}}
    });
    bad_words.words.push_back(WendWordPath{
        "PIG", {Position{2, 0}, Position{2, 1}, Position{2, 2}}
    });
    auto v1 = WendValidator::validate(board, bad_words, lexicon);
    assert(!v1.is_valid);

    // Overlapping cells
    WendSolution overlapping;
    overlapping.words.push_back(WendWordPath{
        "FOR", {Position{0, 0}, Position{0, 1}, Position{0, 2}}
    });
    overlapping.words.push_back(WendWordPath{
        "VAN", {Position{0, 0}, Position{1, 1}, Position{1, 2}} // overlaps (0,0)
    });
    overlapping.words.push_back(WendWordPath{
        "PIG", {Position{2, 0}, Position{2, 1}, Position{2, 2}}
    });
    auto v2 = WendValidator::validate(board, overlapping, lexicon);
    assert(!v2.is_valid);

    // Non-orthogonal step
    WendSolution non_ortho;
    non_ortho.words.push_back(WendWordPath{
        "FOR", {Position{0, 0}, Position{1, 1}, Position{0, 2}} // diagonal!
    });
    non_ortho.words.push_back(WendWordPath{
        "VAN", {Position{1, 0}, Position{0, 1}, Position{1, 2}}
    });
    non_ortho.words.push_back(WendWordPath{
        "PIG", {Position{2, 0}, Position{2, 1}, Position{2, 2}}
    });
    auto v3 = WendValidator::validate(board, non_ortho, lexicon);
    assert(!v3.is_valid);
}

void test_wend_masked_void_cells() {
    std::cout << "Testing Wend with masked / void cells ('.')..." << std::endl;

    // 3x3 board with center void cell '.'
    // C S T
    // A . O
    // R T P
    // Target lengths: 4 4 (CART and STOP)
    std::string grid_str = "3 3\nC S T\nA . O\nR T P\nLENGTHS: 4 4\n";
    auto parse_res = WendBoard::parse_from_string(grid_str);
    assert(parse_res.success);
    assert(parse_res.board.has_value());
    const auto& board = *parse_res.board;
    assert(board.is_void(1, 1));
    assert(!board.is_void(0, 0));
    assert(board.letter_count() == 8);

    Trie lexicon = Trie::get_default_lexicon();
    WendSolver solver(board, lexicon);
    auto solve_res = solver.solve();

    assert(solve_res.status == SolveStatus::UNIQUE_SOLUTION);
    assert(solve_res.solutions.size() == 1);

    const auto& sol = solve_res.solutions[0];
    assert(sol.words.size() == 2);

    auto val_res = WendValidator::validate(board, sol, lexicon);
    assert(val_res.is_valid);

    // Negative check: solution attempting to visit center void cell must be rejected
    WendSolution invalid_sol = sol;
    invalid_sol.words[0].path[3] = Position{1, 1}; // step into void
    auto val_bad = WendValidator::validate(board, invalid_sol, lexicon);
    assert(!val_bad.is_valid);
}

int main() {
    test_wend_parser();
    test_wend_distinct_path_retention();
    test_wend_solve_unique();
    test_wend_repeated_letters();
    test_wend_ambiguous();
    test_wend_no_solution();
    test_wend_validator_negative();
    test_wend_masked_void_cells();

    std::cout << "All Wend tests passed successfully!" << std::endl;
    return 0;
}
