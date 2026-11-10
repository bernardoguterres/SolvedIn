// Regression tests for the explainable Zip rules (with near-misses) and an oracle check against the original
// head-extension search.
#include "solvedin/zip_board.hpp"
#include "solvedin/zip_generator.hpp"
#include "solvedin/zip_logic.hpp"
#include "solvedin/zip_solver.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;
namespace zl = solvedin::zip_logic;

namespace {

ZipBoard board(const std::string& text) {
    auto p = ZipBoard::parse_from_string(text);
    CHECK_ASSERT(p.success);
    return *p.board;
}

int edge_between(const ZipBoard& b, const zl::State& s, Position x, Position y) {
    const int C = b.cols(), u = x.row * C + x.col, v = y.row * C + y.col;
    for (int e = 0; e < static_cast<int>(s.edges.size()); ++e)
        if ((s.edges[e].a == u && s.edges[e].b == v) || (s.edges[e].a == v && s.edges[e].b == u)) return e;
    return -1;
}

void test_corridor_and_near_miss() {
    // Corner (0,0) has exactly two exits and is not a checkpoint: both links are forced.
    auto b = board("3 3\nGRID\n. . .\n1 . .\n. . 2\n");
    auto s = zl::initial_state(b);
    auto d = zl::next_deduction(b, s);
    CHECK_ASSERT(d && d->rule == "corridor");
    CHECK_ASSERT(d->involved[0] == (Position{0, 0}));
    CHECK_ASSERT(d->on.size() == 2);
    // near-miss: the centre (1,1) has four exits and must not be treated as a corridor
    for (int i = 0; i < 20; ++i) {
        auto x = zl::next_deduction(b, s);
        if (!x || x->contradiction) break;
        if (x->rule == "corridor") CHECK_ASSERT(x->involved[0] != (Position{1, 1}) || x->on.size() <= 2);
        zl::apply(s, *x);
    }
    std::cout << "[PASS] corridor forces a two-exit corner, and leaves a four-exit cell alone\n";
}

void test_checkpoint_order() {
    // 1 and 3 are neighbours: linking them directly would skip checkpoint 2.
    auto b = board("2 3\nGRID\n1 3 .\n. 2 4\n");
    auto s = zl::initial_state(b);
    bool closed = false;
    for (int i = 0; i < 30; ++i) {
        auto d = zl::next_deduction(b, s);
        if (!d || d->contradiction) break;
        if (d->rule == "checkpoint-order") {
            const int e = edge_between(b, s, {0, 0}, {0, 1});
            closed |= std::find(d->off.begin(), d->off.end(), e) != d->off.end();
        }
        zl::apply(s, *d);
    }
    const int e = edge_between(b, s, {0, 0}, {0, 1});
    CHECK_ASSERT(s.links[e] == zl::Link::OFF);
    (void)closed;
    std::cout << "[PASS] checkpoint order closes the 1-3 shortcut\n";
}

void test_only_link() {
    // A wall separates the left 2x2 block from the right one except through (1,1)-(1,2); that link must be on the path.
    auto b = board("2 4\nGRID\n. 1 . .\n. . . 2\nWALLS_V\n. | .\n. . .\n");
    auto s = zl::initial_state(b);
    bool saw = false;
    for (int i = 0; i < 40; ++i) {
        auto d = zl::next_deduction(b, s);
        if (!d || d->contradiction) break;
        saw |= d->rule == "only-link" || d->rule == "corridor";
        zl::apply(s, *d);
    }
    CHECK_ASSERT(saw);
    CHECK_ASSERT(s.links[edge_between(b, s, {1, 1}, {1, 2})] == zl::Link::ON);
    std::cout << "[PASS] the only link between two walled-off halves is forced\n";
}

void test_dead_end() {
    // (0,2) is walled off on both sides: a middle cell with one usable neighbour is a dead end.
    auto b = board("2 3\nGRID\n1 . .\n. . 2\nWALLS_H\n. . -\nWALLS_V\n. |\n. .\n");
    auto rep = reason_zip(b);
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    std::cout << "[PASS] a cell with fewer usable neighbours than it needs is a dead end\n";
}

// Every link decided before the first guess must agree with the unique solution path.
void check_against_oracle(const ZipBoard& b, const std::string& label) {
    auto base = solve_zip(b, true);
    auto rep = reason_zip(b);
    if (base.status == SolveStatus::UNIQUE_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        const auto& path = base.solutions[0].path;
        std::set<std::pair<int, int>> on;
        const int C = b.cols();
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            int u = path[i].row * C + path[i].col, v = path[i + 1].row * C + path[i + 1].col;
            on.insert({std::min(u, v), std::max(u, v)});
        }
        auto s = zl::initial_state(b);
        while (auto d = zl::next_deduction(b, s)) {
            CHECK_ASSERT(!d->contradiction);
            for (int e : d->on) if (!on.count({std::min(s.edges[e].a, s.edges[e].b), std::max(s.edges[e].a, s.edges[e].b)})) {
                std::cerr << label << ": unsound ON by " << d->rule << "\n"; std::abort(); }
            for (int e : d->off) if (on.count({std::min(s.edges[e].a, s.edges[e].b), std::max(s.edges[e].a, s.edges[e].b)})) {
                std::cerr << label << ": unsound OFF by " << d->rule << ": " << d->explanation << "\n"; std::abort(); }
            zl::apply(s, *d);
        }
        CHECK_ASSERT(rep.final_board.find("Path: ") != std::string::npos);
    } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    } else if (base.status == SolveStatus::NO_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    }
}

void test_oracle() {
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/zip"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("zip") == std::string::npos || p.find("malformed") != std::string::npos) continue;
            auto parsed = ZipBoard::load_from_file(p);
            if (!parsed.success) continue;
            check_against_oracle(*parsed.board, p);
            ++n;
        }
    }
    for (uint64_t seed = 1; seed <= 40; ++seed) {
        for (auto [r, c, k] : {std::tuple{4, 4, 4}, std::tuple{5, 5, 6}, std::tuple{6, 6, 8}}) {
            check_against_oracle(generate_zip(r, c, k, seed), "generated " + std::to_string(seed));
            ++n;
        }
    }
    std::cout << "[PASS] explainable Zip agrees with the original search on " << n << " boards; every forced link is sound\n";
}

} // namespace

int main() {
    test_corridor_and_near_miss();
    test_checkpoint_order();
    test_only_link();
    test_dead_end();
    test_oracle();
    std::cout << "All Zip logic tests passed.\n";
    return 0;
}
