// Regression tests for the explainable Queens rules (with near-misses) and an oracle check against the
// bitset solver and the generator's planted queens.
#include "solvedin/board.hpp"
#include "solvedin/queens_generator.hpp"
#include "solvedin/queens_logic.hpp"
#include "solvedin/queens_solver.hpp"

#include <algorithm>
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
namespace ql = solvedin::queens_logic;

namespace {

Board board(const std::string& text) {
    auto p = Board::parse_from_string(text);
    CHECK_ASSERT(p.success);
    return *p.board;
}

bool has(const std::vector<Position>& v, Position p) { return std::find(v.begin(), v.end(), p) != v.end(); }

void test_region_line_lock() {
    // Region 1 lives only in column 4, so its queen is in column 4: other regions' cells there go.
    auto b = board("5\n0 0 0 0 1\n0 0 0 0 1\n2 2 2 2 2\n3 3 3 3 2\n4 4 4 4 2\n");
    auto d = ql::next_deduction(b, ql::initial_state(b));
    CHECK_ASSERT(d && d->rule == "region-line-lock");
    CHECK_ASSERT(d->eliminated.size() == 3 && has(d->eliminated, {2, 4}) && has(d->eliminated, {3, 4}) && has(d->eliminated, {4, 4}));
    // near-miss: region 1 spread over two columns must not trigger a column-4 lock for region 1
    auto b2 = board("5\n0 0 0 0 1\n0 0 0 1 0\n2 2 2 2 2\n3 3 3 3 2\n4 4 4 4 2\n");
    auto d2 = ql::next_deduction(b2, ql::initial_state(b2));
    CHECK_ASSERT(!d2 || d2->explanation.find("region 1 (") == std::string::npos);
    std::cout << "[PASS] region-to-line lock, and its near-miss\n";
}

void test_line_region_lock() {
    // Column 0 lies entirely in region 0, so region 0's queen is in column 0.
    auto b = board("4\n0 0 1 1\n0 2 2 1\n0 2 3 3\n0 0 3 3\n");
    auto s = ql::initial_state(b);
    auto d = ql::next_deduction(b, s);
    CHECK_ASSERT(d && d->rule == "line-region-lock");
    CHECK_ASSERT(has(d->eliminated, {0, 1}) && has(d->eliminated, {3, 1}) && !has(d->eliminated, {1, 0}));
    std::cout << "[PASS] line-to-region lock\n";
}

void test_would_empty_group() {
    // Region 1 is the 2x1 block (0,1)-(1,1); a queen at (0,0), (1,0), (0,2) or (1,2) would touch both cells.
    auto b = board("4\n0 1 2 2\n0 1 2 2\n0 3 3 3\n0 3 3 3\n");
    auto s = ql::initial_state(b);
    // apply earlier rules until the empty-group rule is the one that fires
    for (int i = 0; i < 10; ++i) {
        auto d = ql::next_deduction(b, s);
        CHECK_ASSERT(d && !d->contradiction);
        if (d->rule == "would-empty-group") {
            CHECK_ASSERT(!d->eliminated.empty());
            for (auto p : d->eliminated) CHECK_ASSERT(p.col == 0 || p.col == 2 || p.row <= 1);
            std::cout << "[PASS] would-empty-group eliminates cells that attack a whole group\n";
            return;
        }
        ql::apply(s, *d);
    }
    CHECK_ASSERT(false);
}

// Root deductions must keep every queen of a known solution (sound even when the board has several solutions).
void check_planted(const Board& b, const std::vector<Position>& queens, const std::string& label) {
    ReasoningOptions o;
    o.mode = ReasoningMode::DEDUCE;
    auto s = ql::initial_state(b);
    while (auto d = ql::next_deduction(b, s)) {
        CHECK_ASSERT(!d->contradiction);
        for (auto p : d->eliminated)
            if (has(queens, p)) { std::cerr << label << ": " << d->rule << " removed a solution queen at " << cell_name(p) << "\n"; std::abort(); }
        ql::apply(s, *d);
    }
}

void check_against_bitset(const Board& b, const std::string& label) {
    auto base = solve_queens(b, true, SolverVariant::BITSET);
    auto rep = reason_queens(b);
    if (base.status == SolveStatus::UNIQUE_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        check_planted(b, base.solutions[0].queens, label);
    } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    } else if (base.status == SolveStatus::NO_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    }
}

void test_oracle() {
    int n = 0;
    for (const char* f : {"fixtures/valid_8x8_unique.txt", "fixtures/valid_4x4_multiple.txt", "fixtures/valid_no_solution.txt",
                          "fixtures/valid_long_diagonal.txt", "fixtures/corpus/queens/easy/queens_8x8_101.txt",
                          "fixtures/corpus/queens/easy/queens_10x10_303.txt", "fixtures/corpus/queens/medium/queens_16x16_404.txt"}) {
        auto p = Board::load_from_file(f);
        CHECK_ASSERT(p.success);
        check_against_bitset(*p.board, f);
        ++n;
    }
    for (int size : {6, 7, 8, 9, 10}) {
        for (uint64_t seed = 1; seed <= 30; ++seed) {
            auto g = QueensGenerator::generate(size, seed);
            check_planted(g.board, g.planted_queens, "generated " + std::to_string(size) + "/" + std::to_string(seed));
            check_against_bitset(g.board, "generated");
            ++n;
        }
    }
    std::cout << "[PASS] explainable Queens agrees with the bitset solver on " << n << " boards; no deduction removes a solution queen\n";
}

void test_modes() {
    auto p = Board::load_from_file("fixtures/valid_8x8_unique.txt");
    CHECK_ASSERT(p.success);
    ReasoningOptions o;
    o.mode = ReasoningMode::HINT;
    auto h = reason_queens(*p.board, o);
    CHECK_ASSERT(h.steps.size() == 1 && h.steps[0].kind == StepKind::DEDUCTION);
    o.mode = ReasoningMode::DEDUCE;
    auto d = reason_queens(*p.board, o);
    for (const auto& s : d.steps) CHECK_ASSERT(s.kind != StepKind::BRANCH);
    o.mode = ReasoningMode::EXPLAIN;
    auto e = reason_queens(*p.board, o);
    CHECK_ASSERT(e.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || e.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
    auto m = reason_queens(*Board::load_from_file("fixtures/valid_4x4_multiple.txt").board, o);
    CHECK_ASSERT(m.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS && m.solutions_text.size() == 2);
    std::cout << "[PASS] Queens hint / deduce / explain modes\n";
}

} // namespace

int main() {
    test_region_line_lock();
    test_line_region_lock();
    test_would_empty_group();
    test_oracle();
    test_modes();
    std::cout << "All Queens logic tests passed.\n";
    return 0;
}
