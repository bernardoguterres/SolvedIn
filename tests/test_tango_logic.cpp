// Regression tests for the explainable Tango rules, including near-misses where a rule must do nothing,
// and an oracle check against the original propagate-and-branch solver.
#include "solvedin/tango_board.hpp"
#include "solvedin/tango_generator.hpp"
#include "solvedin/tango_logic.hpp"
#include "solvedin/tango_solver.hpp"

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
namespace tl = solvedin::tango_logic;

namespace {

// rows: 6 strings of 6 chars (S/M/.), h: 6 strings of 5 chars, v: 5 strings of 6 chars ('.', '=', 'x')
TangoBoard board(const std::vector<std::string>& rows, std::vector<std::string> h = {}, std::vector<std::string> v = {}) {
    if (h.empty()) h.assign(6, ".....");
    if (v.empty()) v.assign(5, "......");
    std::string s = "6\nGRID\n";
    auto spaced = [](const std::string& x) { std::string o; for (char c : x) { o += c; o += ' '; } return o + "\n"; };
    for (auto& r : rows) s += spaced(r);
    s += "HORIZONTAL\n";
    for (auto& r : h) s += spaced(r);
    s += "VERTICAL\n";
    for (auto& r : v) s += spaced(r);
    auto p = TangoBoard::parse_from_string(s);
    CHECK_ASSERT(p.success);
    return *p.board;
}

std::set<std::string> sets_of(const tl::Deduction& d) {
    std::set<std::string> out;
    for (auto& [p, v] : d.sets) out.insert(cell_name(p) + (v == tl::S ? "S" : "M"));
    return out;
}

const std::vector<std::string> EMPTY5 = {"......", "......", "......", "......", "......"};

void test_line_pattern_example() {
    // S ? ? ? ? S: the remaining sun cannot go at position 1 or 4 (it would leave three moons), so both are moons.
    std::vector<std::string> rows = {"S....S"}; rows.insert(rows.end(), EMPTY5.begin(), EMPTY5.end());
    auto b = board(rows);
    auto d = tl::next_deduction(b, tl::initial_domains(b));
    CHECK_ASSERT(d && !d->contradiction);
    CHECK_ASSERT(d->rule == "line-patterns");
    CHECK_ASSERT((sets_of(*d) == std::set<std::string>{"(0, 1)M", "(0, 4)M"}));
    CHECK_ASSERT(d->explanation.find("SMSMMS") != std::string::npos && d->explanation.find("SMMSMS") != std::string::npos);
    std::cout << "[PASS] line patterns: S????S forces moons at positions 1 and 4\n";
}

void test_line_pattern_near_miss() {
    std::vector<std::string> rows = {"S....."}; rows.insert(rows.end(), EMPTY5.begin(), EMPTY5.end());
    auto b = board(rows);
    CHECK_ASSERT(!tl::next_deduction(b, tl::initial_domains(b)));  // a lone sun proves nothing
    std::cout << "[PASS] line patterns near-miss: S????? proves nothing\n";
}

void test_x_pair_count() {
    // Row 0: S . . S then an open 'x' pair at columns 4-5 supplies the third sun, so columns 1 and 2 are moons.
    std::vector<std::string> rows = {"S..S.."}; rows.insert(rows.end(), EMPTY5.begin(), EMPTY5.end());
    auto b = board(rows, {"....x", ".....", ".....", ".....", ".....", "....."});
    auto d = tl::next_deduction(b, tl::initial_domains(b));
    CHECK_ASSERT(d && d->rule == "x-pair-count");
    CHECK_ASSERT((sets_of(*d) == std::set<std::string>{"(0, 1)M", "(0, 2)M"}));
    // near-miss: without the sign nothing is forced
    auto b2 = board(rows);
    CHECK_ASSERT(!tl::next_deduction(b2, tl::initial_domains(b2)));
    std::cout << "[PASS] x-pair count, and its near-miss without the sign\n";
}

void test_equal_pair_count() {
    // Row 0: S M . = . S . has two suns; the open '=' pair cannot be two more suns, so both are moons.
    std::vector<std::string> rows = {"SM..S."}; rows.insert(rows.end(), EMPTY5.begin(), EMPTY5.end());
    auto b = board(rows, {"..=..", ".....", ".....", ".....", ".....", "....."});
    auto d = tl::next_deduction(b, tl::initial_domains(b));
    CHECK_ASSERT(d && d->rule == "equal-pair-count");
    CHECK_ASSERT((sets_of(*d) == std::set<std::string>{"(0, 2)M", "(0, 3)M"}));
    std::cout << "[PASS] equal-pair count\n";
}

void test_signs_pairs_gaps() {
    std::vector<std::string> rows = {"S.....", "......", "M.M...", "......", "......", "......"};
    auto b = board(rows, {"=....", ".....", ".....", ".....", ".....", "....."});
    auto d = tl::next_deduction(b, tl::initial_domains(b));
    CHECK_ASSERT(d && d->rule == "equal-sign" && (sets_of(*d) == std::set<std::string>{"(0, 1)S"}));
    auto b2 = board(rows);
    auto d2 = tl::next_deduction(b2, tl::initial_domains(b2));
    CHECK_ASSERT(d2 && d2->rule == "gap" && (sets_of(*d2) == std::set<std::string>{"(2, 1)S"}));
    std::cout << "[PASS] sign and gap rules\n";
}

void test_contradiction_detected() {
    // S . S with '=' between the last two: the sign forces a sun into the gap, making three suns in a row.
    std::vector<std::string> rows = {"S.S...", "......", "......", "......", "......", "......"};
    auto b = board(rows, {".=...", ".....", ".....", ".....", ".....", "....."});
    auto d = tl::next_deduction(b, tl::initial_domains(b));
    CHECK_ASSERT(d && !d->contradiction && d->rule == "equal-sign");
    auto rep = reason_tango(b);
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    std::cout << "[PASS] three in a row is reported as a contradiction\n";
}

// Oracle: the explainable solver agrees with the original solver, and every deduction made before the first
// guess agrees with the unique solution (soundness).
void check_against_oracle(const TangoBoard& b, const std::string& label) {
    auto base = solve_tango(b, true);
    ReasoningOptions opt;
    auto rep = reason_tango(b, opt);
    switch (base.status) {
    case SolveStatus::UNIQUE_SOLUTION: {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        const auto& sol = base.solutions[0].grid;
        for (const auto& s : rep.steps) {
            if (s.kind != StepKind::DEDUCTION || s.depth != 0) continue;
            for (const auto& ch : s.changes) {
                if (sol[ch.cell.row][ch.cell.col] != ch.value[0]) {
                    std::cerr << label << ": unsound step " << s.rule << " " << s.explanation << "\n";
                    std::abort();
                }
            }
        }
        std::string t;
        for (auto& row : sol) { for (size_t i = 0; i < row.size(); ++i) { t += row[i]; if (i + 1 < row.size()) t += ' '; } t += '\n'; }
        CHECK_ASSERT(rep.final_board == t);
        break;
    }
    case SolveStatus::MULTIPLE_SOLUTIONS: CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS); break;
    case SolveStatus::NO_SOLUTION: CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION); break;
    case SolveStatus::INCONCLUSIVE: break;
    }
}

void test_oracle_fixtures_and_generated() {
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/tango"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("tango") == std::string::npos || p.find("malformed") != std::string::npos) continue;
            auto parsed = TangoBoard::load_from_file(p);
            if (!parsed.success) continue;
            check_against_oracle(*parsed.board, p);
            ++n;
        }
    }
    for (uint64_t seed = 1; seed <= 60; ++seed) {
        for (int givens : {2, 5, 8}) {
            check_against_oracle(generate_tango(seed, givens, 4), "seed " + std::to_string(seed));
            ++n;
        }
    }
    // Under-constrained boards (multiple solutions) exercise the uniqueness search.
    std::vector<std::string> rows = {"S.....", "......", "......", "......", "......", "......"};
    check_against_oracle(board(rows), "nearly empty");
    std::cout << "[PASS] explainable Tango agrees with the original solver on " << n + 1 << " boards (and every forced step is sound)\n";
}

void test_modes() {
    auto parsed = TangoBoard::load_from_file("fixtures/corpus/tango/hard/tango_hard_1.txt");
    CHECK_ASSERT(parsed.success);
    ReasoningOptions o;
    o.mode = ReasoningMode::DEDUCE;
    auto d = reason_tango(*parsed.board, o);
    CHECK_ASSERT(d.outcome == ReasoningOutcome::STALLED);
    CHECK_ASSERT(d.steps.back().kind == StepKind::STALL);
    for (const auto& s : d.steps) CHECK_ASSERT(s.kind != StepKind::BRANCH);  // deduce never guesses
    o.mode = ReasoningMode::HINT;
    auto h = reason_tango(*parsed.board, o);
    CHECK_ASSERT(h.outcome == ReasoningOutcome::HINT && h.steps.size() == 1 && h.steps[0].kind == StepKind::DEDUCTION);
    o.mode = ReasoningMode::EXPLAIN;
    auto e = reason_tango(*parsed.board, o);
    CHECK_ASSERT(e.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
    bool saw_branch = false, saw_dead_end = false;
    for (const auto& s : e.steps) { saw_branch |= s.kind == StepKind::BRANCH; saw_dead_end |= s.kind == StepKind::CONTRADICTION; }
    CHECK_ASSERT(saw_branch && saw_dead_end && e.stats.failed_branches > 0);
    // Here the solution sits on the last open branch, so the dead ends already prove uniqueness (0 verify nodes).
    // An under-constrained board needs the separate verify-unique phase to find the second solution.
    std::vector<std::string> rows = {"S.....", "......", "......", "......", "......", "......"};
    auto m = reason_tango(board(rows), o);
    CHECK_ASSERT(m.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS && m.stats.uniqueness_nodes > 0);
    bool saw_verify = false;
    for (const auto& s : m.steps) saw_verify |= s.phase == SearchPhase::VERIFY_UNIQUE;
    CHECK_ASSERT(saw_verify);
    std::cout << "[PASS] hint / deduce / explain modes keep forced moves, guesses and the uniqueness check distinct\n";
}

} // namespace

int main() {
    test_line_pattern_example();
    test_line_pattern_near_miss();
    test_x_pair_count();
    test_equal_pair_count();
    test_signs_pairs_gaps();
    test_contradiction_detected();
    test_oracle_fixtures_and_generated();
    test_modes();
    std::cout << "All Tango logic tests passed.\n";
    return 0;
}
