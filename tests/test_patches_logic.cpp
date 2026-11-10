// Regression tests for the explainable Patches rules (with near-misses) and an oracle check against the
// exact-cover solver.
#include "solvedin/patches_board.hpp"
#include "solvedin/patches_logic.hpp"
#include "solvedin/patches_solver.hpp"

#include <algorithm>
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
namespace pl = solvedin::patches_logic;

namespace {

PatchesBoard board(const std::string& text) {
    auto p = PatchesBoard::parse_from_string(text);
    CHECK_ASSERT(p.success);
    return *p.board;
}

std::optional<pl::Deduction> first_with_rule(const PatchesBoard& b, pl::State& s, const std::string& rule) {
    for (int i = 0; i < 50; ++i) {
        auto d = pl::next_deduction(b, s);
        if (!d || d->contradiction) return std::nullopt;
        if (d->rule == rule) return d;
        pl::apply(b, s, *d);
    }
    return std::nullopt;
}

void test_single_rectangle_and_only_cover() {
    // 1x3 board, clue '3' fills it: one rectangle.
    auto b = board("1 3\nGRID\n3 . .\n");
    auto s = pl::initial_state(b);
    auto d = pl::next_deduction(b, s);
    CHECK_ASSERT(d && d->rule == "single-rectangle" && d->place.size() == 1);
    // 2x2 board with two '2' clues on the diagonal: (0,1) can be reached by both, so no single cover at first.
    auto b2 = board("2 2\nGRID\n2 .\n. 2\n");
    auto s2 = pl::initial_state(b2);
    auto d2 = pl::next_deduction(b2, s2);
    CHECK_ASSERT(!d2);  // near-miss: both clues reach every free cell, two solutions, nothing is forced
    std::cout << "[PASS] single rectangle, and the near-miss where two clues share every free cell\n";
}

void test_reserved_cell() {
    // Clue W ('3W' = 1x3) at (0,1) on a 2x3 board: every 1x3 through (0,1) is row 0, so (0,0) and (0,2) are reserved.
    auto b = board("2 3\nGRID\n.  3W .\n.  3  .\n");
    auto s = pl::initial_state(b);
    auto d = pl::next_deduction(b, s);
    CHECK_ASSERT(d && !d->contradiction);
    auto rep = reason_patches(b);
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION);
    std::cout << "[PASS] reserved cells / single rectangles solve a 2x3 board without guessing\n";
}

void test_only_clue() {
    // Clue A ('?') at (0,0) and clue B ('2') at (1,2): B's 2-cell rectangles cannot reach (1,0), so only A can.
    // A's rectangles that miss (1,0) (the 1x1, 1x2 and 1x3 in row 0) must go.
    auto b = board("2 3\nGRID\n?  .  .\n.  .  2\n");
    auto s = pl::initial_state(b);
    auto d = first_with_rule(b, s, "only-clue");
    CHECK_ASSERT(d);
    const Position q = d->involved[0];  // (0,1) and (1,0) are both out of B's reach; the scan finds (0,1) first
    CHECK_ASSERT(q == (Position{0, 1}) || q == (Position{1, 0}));
    for (const auto& r : s.rects) if (r.clue == 1) CHECK_ASSERT(!r.covers(q.row, q.col));  // B cannot reach it
    CHECK_ASSERT(!d->eliminate.empty());
    for (int i : d->eliminate) {
        CHECK_ASSERT(s.rects[static_cast<size_t>(i)].clue == 0);
        CHECK_ASSERT(!s.rects[static_cast<size_t>(i)].covers(q.row, q.col));
    }
    std::cout << "[PASS] only-clue: a cell only one clue can reach constrains that clue's rectangles\n";
}

void check_against_oracle(const PatchesBoard& b, const std::string& label) {
    auto base = solve_patches(b, true);
    auto rep = reason_patches(b);
    if (base.status == SolveStatus::UNIQUE_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        // soundness: no forced step may remove or contradict the unique solution's rectangle of any clue
        const auto& sol = base.solutions[0];
        auto s = pl::initial_state(b);
        auto in_solution = [&](const pl::Rect& r) {
            const auto& cl = b.clues()[static_cast<size_t>(r.clue)];
            for (const auto& p : sol.patches) {
                if (p.clue_pos != cl.pos) continue;
                if (static_cast<int>(p.cells.size()) != r.h * r.w) return false;
                return std::all_of(p.cells.begin(), p.cells.end(), [&](Position q) { return r.covers(q.row, q.col); });
            }
            return false;
        };
        while (auto d = pl::next_deduction(b, s)) {
            CHECK_ASSERT(!d->contradiction);
            for (int i : d->eliminate) if (in_solution(s.rects[static_cast<size_t>(i)])) { std::cerr << label << ": unsound " << d->rule << "\n"; std::abort(); }
            for (int i : d->place) if (!in_solution(s.rects[static_cast<size_t>(i)])) { std::cerr << label << ": unsound place " << d->rule << "\n"; std::abort(); }
            pl::apply(b, s, *d);
        }
    } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    } else if (base.status == SolveStatus::NO_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    }
}

void test_oracle() {
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/patches"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("patches") == std::string::npos || p.find("malformed") != std::string::npos) continue;
            auto parsed = PatchesBoard::load_from_file(p);
            if (!parsed.success) continue;
            check_against_oracle(*parsed.board, p);
            ++n;
        }
    }
    // small unsized boards exercise multiple solutions and the uniqueness search
    for (const char* t : {"2 2\nGRID\n2 .\n. 2\n", "2 3\nGRID\n. ? .\n. ? .\n", "3 3\nGRID\n3  .  .\n.  .  .\n.  .  6\n", "1 4\nGRID\n? . . ?\n"}) {
        check_against_oracle(board(t), t);
        ++n;
    }
    std::cout << "[PASS] explainable Patches agrees with the exact-cover solver on " << n << " boards; every forced step is sound\n";
}

} // namespace

int main() {
    test_single_rectangle_and_only_cover();
    test_reserved_cell();
    test_only_clue();
    test_oracle();
    std::cout << "All Patches logic tests passed.\n";
    return 0;
}
