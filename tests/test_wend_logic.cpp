// Regression tests for the explainable Wend rules and an oracle check against the exact-cover solver.
#include "solvedin/trie.hpp"
#include "solvedin/wend_board.hpp"
#include "solvedin/wend_logic.hpp"
#include "solvedin/wend_solver.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#define CHECK_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "Assertion failed: " #expr " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort(); \
        } \
    } while (0)

using namespace solvedin;
namespace wl = solvedin::wend_logic;

namespace {

Trie lexicon(const std::string& words) {
    std::istringstream is(words);
    return Trie::load_from_stream(is);
}

WendBoard board(const std::string& text) {
    auto p = WendBoard::parse_from_string(text);
    CHECK_ASSERT(p.success);
    return *p.board;
}

void test_scarce_cell() {
    auto b = board("2 3\nGRID\nC A T\nD O G\nLENGTHS: 3 3\n");
    auto lex = lexicon("CAT\nDOG\nCOD\nTAG\n");
    auto s = wl::initial_state(b, lex);
    auto d = wl::next_deduction(b, s);
    CHECK_ASSERT(d && !d->contradiction);
    CHECK_ASSERT(d->rule == "scarce-cell" || d->rule == "would-strand");
    auto rep = reason_wend(b, lex, "test");
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK ||
                 rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    CHECK_ASSERT(!rep.notes.empty() && rep.notes[0].find("dictionary") != std::string::npos);  // the assumption is stated
    std::cout << "[PASS] scarce cell, and the dictionary assumption is stated\n";
}

void test_dictionary_miss_is_not_a_wrong_answer() {
    // DOG is missing from the dictionary: the solver must report no solution, not invent a cover.
    auto b = board("2 3\nGRID\nC A T\nD O G\nLENGTHS: 3 3\n");
    auto rep = reason_wend(b, lexicon("CAT\n"), "test");
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    std::cout << "[PASS] a dictionary miss gives NO_SOLUTION\n";
}

void test_lookahead() {
    // Lengths 3 and 3 on a 2x3 board.
    auto b = board("2 3\nGRID\nA B C\nD E F\nLENGTHS: 3 3\n");
    auto lex = lexicon("ABE\nABC\nDEF\nCFE\nADE\n");
    auto rep = reason_wend(b, lex, "test");
    // ADE would leave B with no word path: the look-ahead removes it before anything else, then two scarce cells finish.
    CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION);
    CHECK_ASSERT(!rep.steps.empty() && rep.steps[0].rule == "would-strand" && rep.steps[0].explanation.find("ADE") != std::string::npos);
    std::cout << "[PASS] look-ahead removes a word path that would strand a cell\n";
}

void check_against_oracle(const WendBoard& b, const Trie& lex, const std::string& label) {
    WendSolver solver(b, lex);
    auto base = solver.solve(true);
    auto rep = reason_wend(b, lex, label);
    if (base.status == SolveStatus::UNIQUE_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::SOLVED_BY_DEDUCTION || rep.outcome == ReasoningOutcome::SOLVED_WITH_FALLBACK);
        const auto& sol = base.solutions[0].words;
        auto in_sol = [&](const WendWordPath& p) {
            return std::any_of(sol.begin(), sol.end(), [&](const WendWordPath& q) {
                auto rp = p.path; std::reverse(rp.begin(), rp.end());
                return q.path == p.path || q.path == rp;
            });
        };
        auto s = wl::initial_state(b, lex);
        while (auto d = wl::next_deduction(b, s)) {
            CHECK_ASSERT(!d->contradiction);
            for (int i : d->place) if (!in_sol(s.paths[static_cast<size_t>(i)])) { std::cerr << label << ": unsound " << d->rule << "\n"; std::abort(); }
            for (int i : d->eliminate) if (in_sol(s.paths[static_cast<size_t>(i)])) { std::cerr << label << ": unsound elimination " << d->rule << "\n"; std::abort(); }
            wl::apply(b, s, *d);
        }
    } else if (base.status == SolveStatus::MULTIPLE_SOLUTIONS) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::MULTIPLE_SOLUTIONS);
    } else if (base.status == SolveStatus::NO_SOLUTION) {
        CHECK_ASSERT(rep.outcome == ReasoningOutcome::NO_SOLUTION);
    }
}

void test_oracle() {
    auto lex = Trie::get_default_lexicon();
    int n = 0;
    for (const auto& dir : {"fixtures", "fixtures/corpus/wend"}) {
        for (const auto& e : std::filesystem::recursive_directory_iterator(dir)) {
            const std::string p = e.path().string();
            if (!e.is_regular_file() || p.find("wend") == std::string::npos || p.find("malformed") != std::string::npos || p.find("lexicon") != std::string::npos) continue;
            auto parsed = WendBoard::load_from_file(p);
            if (!parsed.success) continue;
            check_against_oracle(*parsed.board, lex, p);
            ++n;
        }
    }
    std::cout << "[PASS] explainable Wend agrees with the exact-cover solver on " << n << " boards; every forced step is sound\n";
}

} // namespace

int main() {
    test_scarce_cell();
    test_dictionary_miss_is_not_a_wrong_answer();
    test_lookahead();
    test_oracle();
    std::cout << "All Wend logic tests passed.\n";
    return 0;
}
