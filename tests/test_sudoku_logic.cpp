// Regression tests for the explainable Mini Sudoku rules (with near-misses) and an oracle check against the
// original singles-and-branch solver.
#include "solvedin/sudoku_board.hpp"
#include "solvedin/sudoku_generator.hpp"
#include "solvedin/sudoku_logic.hpp"
#include "solvedin/sudoku_solver.hpp"

#include <cstdlib>
#include <filesystem>
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
namespace sl = solvedin::sudoku_logic;

namespace {

SudokuBoard load(const std::string& f) {
    auto p = SudokuBoard::load_from_file(f);
    CHECK_ASSERT(p.success);
    return *p.board;
}

// Every deduction made before the first guess must agree with the unique solution.
void check_sound(const SudokuBoard& b, const std::array<std::array<int, 6>, 6>& sol, bool basic, const std::string& label) {
    auto s = sl::initial_state(b);
    while (auto d = sl::next_deduction(s, basic)) {
        CHECK_ASSERT(!d->contradiction);
        for (auto& [p, v] : d->placements)
            if (sol[p.row][p.col] != v) { std::cerr << label << ": unsound " << d->rule << "\n"; std::abort(); }
        for (auto& [p, v] : d->eliminations)
            if (sol[p.row][p.col] == v) { std::cerr << label << ": " << d->rule << " removed the solution digit at " << cell_name(p) << "\n"; std::abort(); }
        sl::apply(s, *d);
    }
}

void test_naked_pair_fixture() {
    auto b = load("fixtures/sudoku_naked_pair.txt");
    ReasoningOptions o;
    o.mode = ReasoningMode::DEDUCE;
    o.basic_rules = true;
    CHECK_ASSERT(reason_sudoku(b, o).outcome == ReasoningOutcome::STALLED);  // near-miss: singles alone stall
    o.basic_rules = false;
    auto r = reason_sudoku(b, o);
    CHECK_ASSERT(r.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION);
    CHECK_ASSERT(r.stats.rule_counts.count("naked-pair"));
    auto base = solve_sudoku(b, true);
    CHECK_ASSERT(base.status == SolveStatus::UNIQUE_SOLUTION);
    check_sound(b, base.solutions[0].grid, false, "naked pair fixture");
    std::cout << "[PASS] naked pair finishes a board on which singles stall\n";
}

void test_locked_candidates_fixture() {
    auto b = load("fixtures/sudoku_locked_candidates.txt");
    auto s = sl::initial_state(b);
    bool saw = false;
    while (auto d = sl::next_deduction(s)) {
        if (d->rule == "locked-box-line") {
            saw = true;
            // eliminations stay on the line and outside the box that holds every candidate
            CHECK_ASSERT(!d->eliminations.empty());
            const int box = SudokuBoard::box_index(d->involved[0].row, d->involved[0].col);
            for (auto& [p, v] : d->eliminations) {
                CHECK_ASSERT(SudokuBoard::box_index(p.row, p.col) != box);
                CHECK_ASSERT(p.row == d->involved[0].row || p.col == d->involved[0].col);
            }
        }
        sl::apply(s, *d);
    }
    CHECK_ASSERT(saw);
    auto base = solve_sudoku(b, true);
    check_sound(b, base.solutions[0].grid, false, "locked fixture");
    std::cout << "[PASS] box-to-line lock eliminates only outside the box, and is sound\n";
}

void test_empty_board_near_miss() {
    auto p = SudokuBoard::parse_from_string("6\n. . . . . .\n. . . . . .\n. . . . . .\n. . . . . .\n. . . . . .\n. . . . . .\n");
    CHECK_ASSERT(p.success);
    CHECK_ASSERT(!sl::next_deduction(sl::initial_state(*p.board)));  // nothing is locked on an empty board
    std::cout << "[PASS] near-miss: no rule fires on an empty board\n";
}

void check_against_oracle(const SudokuBoard& b, const std::string& label) {
    auto base = solve_sudoku(b, true);
    auto rep = reason_sudoku(b);
    if (base.status == SolveStatus::UNIQUE_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        sl::State s;
        s.grid = base.solutions[0].grid;
        CHECK_ASSERT(rep.final_board == sl::render(s));
        check_sound(b, base.solutions[0].grid, false, label);
        check_sound(b, base.solutions[0].grid, true, label);
    } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    } else if (base.status == SolveStatus::NO_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    }
}

void test_oracle() {
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/sudoku"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("sudoku") == std::string::npos || p.find("malformed") != std::string::npos) continue;
            auto parsed = SudokuBoard::load_from_file(p);
            if (!parsed.success) continue;
            check_against_oracle(*parsed.board, p);
            ++n;
        }
    }
    for (uint64_t seed = 1; seed <= 80; ++seed) {
        for (int remove : {20, 26, 30}) {
            check_against_oracle(SudokuGenerator::generate(seed, remove), "generated");
            ++n;
        }
    }
    std::cout << "[PASS] explainable Sudoku agrees with the original solver on " << n << " boards; every forced step is sound\n";
}

} // namespace

int main() {
    test_naked_pair_fixture();
    test_locked_candidates_fixture();
    test_empty_board_near_miss();
    test_oracle();
    std::cout << "All Sudoku logic tests passed.\n";
    return 0;
}
